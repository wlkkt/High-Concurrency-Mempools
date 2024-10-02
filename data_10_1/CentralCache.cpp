#include "CentralCache.h"
#include "PageCache.h"

//定义
CentralCache CentralCache::_sInst;

//实际可以从CentralCache中获取到的内存结点的个数
size_t CentralCache::FetchRangeObj(void*& start, void*& end, size_t batchNum, size_t size)
{
	size_t index = SizeClass::Index(size);
	_spanLists[index]._mtx.lock();//上桶锁
	Span* span = GetOneSpan(_spanLists[index], size);//获取一个非空span

	assert(span);
	assert(span->_freelist);//span为空或者span管理的空间为空均不行

//尝试从span中获取batchNum个对象,若没有这么多对象的话,有多少就给多少
	start = end = span->_freelist;
	size_t actualNum = 1;//已经判断过的自由链表不为空，所以肯定有一个

	size_t i = 0;
	//NextObj(end) != nullptr用于防止actualNum小于bathcNum，循环次数过多时NexeObj(end)中的end为nullptr，导致的报错
	while (i < batchNum - 1 && NextObj(end) != nullptr)
	{
		end = NextObj(end);
		++i;
		++actualNum;
	}
	span->_useCount += actualNum;//当前span中有actualNum个内存结点被分配给ThreadCache

	//更新当前span的自由链表中
	span->_freelist = NextObj(end);
	NextObj(end) = nullptr;

	_spanLists[index]._mtx.unlock();//解桶锁
	return actualNum;
}


//获取非空span
Span* CentralCache::GetOneSpan(SpanList& list, size_t size)
{
	//遍历CentralCache当前桶中的SpanList寻找一个非空的span
	Span* it = list.Begin();
	while (it != list.End())
	{
		if (it->_freelist != nullptr)
		{
			return it;
		}
		else
		{
			it = it->_next;
		}
	}

	//先把进来GetOneSpan前设置的桶锁解除，避免其它线程释放内存时被阻塞
	list._mtx.unlock();

	size_t k = SizeClass::NumMovePage(size);//SizeClass::NumMovePage(size)计算要向PageCache申请管理多少页的span，即k

	//走到这里证明CentralCache当前桶中的SpanList中没有非空的span对象了，需要向PageCache申请
	PageCache::GetInstance()->_pageMtx.lock();//为PageCache整体上锁
	Span* span = PageCache::GetInstance()->NewSpan(k);
	PageCache::GetInstance()->_pageMtx.unlock();//为PageCache整体解锁

	span->_isUse = true;//修改从PageCache获取到的span的状态为正在使用
	span->_objSize = size;//填充该span要被切分出去的内存大小

	//从PageCache中获取的span是没有进行内存切分的，需要进行切分并挂在其自由链表下

	//1、计算span管理下的大块内存的起始和结尾地址
	//起始地址 = 页号 * 页的大小


	char* start = (char*)(span->_PageId << PAGE_SHIFT);//选择char*而不是void*，为了后续+=size的时移动size个字节
	//假设span->_PageId = 5，PAGE_SHIFT = 13，5 >> 13 = 40960（字节）
	//整数值 40960 表示内存中的一个地址位置，通过 (char*) 显示类型转换后，start 就指向了这个内存地址，即span的起始地址

	char* end = (char*)(start + (span->_n << PAGE_SHIFT));//end指向span的结束地址，span管理的内存大小 = span中页的个数 * 页大小 

	//2、将start和end指向的大块内存切成多个小块内存，并尾插至自由链表中（采用尾插，使得即使被切割但在物理上仍为连续空间，加快访问速度）

	//①先切下来一块作为头结点，便于尾插
	span->_freelist = start;
	void* tail = start;
	start += size;

	//循环尾插
	while (start < end)
	{
		NextObj(tail) = start;//当前tail指向的内存块的前4/8个字节存放下一个结点的起始地址，即start指向的结点的地址
		start += size;//更新start
		tail = NextObj(tail);//更新tail
	}

	NextObj(tail) = nullptr;

	//向CentralCache中当前的SpanList头插前要上锁，防止其它线程同时访问当前的SpanList
	list._mtx.lock();
	list.PushFront(span);

	return span;//此时该span已经放在了CentralCache的某个桶的SpanList中了，返回该span的地址即可
}

//将从ThreadCache获得内存结点归还给它们所属的span，因为这些内存结点可能是由CentralCache同一桶中的不同span分配的
//前一个span用完了，从后一个span中获取，同时前一个span可能还会接收从ThreadCache归还回来的内存结点，下次分配时可能又可以从前面的span分配了，多线程考虑的有点多
void CentralCache::ReleaseListToSpans(void* start, size_t size)
{
	size_t index = SizeClass::Index(size);
	_spanLists[index]._mtx.lock();

	//start指向的是一串内存结点的头结点
	while (start)
	{
		void* next = NextObj(start);
		Span* span = PageCache::GetInstance()->MapObjectToSpan(start);//确定要归还的span

		//头插内存结点到span中
		NextObj(start) = span->_freelist;
		span->_freelist = start;

		span->_useCount--;//_useCount--表示有一个分配出去的小块内存回到当前span

		//当前span的_useCount为0表示当前span切分出去的所有小块内存都回来了，直接将整个span还给PageCache,PageCache再尝试进行前后页的合并
		if (span->_useCount == 0)
		{
			_spanLists[index].Erase(span);//将当前的span从CentralCache的某个桶处的SpanList上取下

			//参与到PageCache中进行合并的span不需要自由链表等内容，置空即可
			span->_freelist = nullptr;
			span->_next = nullptr;
			span->_prev = nullptr;

			_spanLists[index]._mtx.unlock();//不用了就解锁,避免对CentralCache中同一桶的锁竞争

			PageCache::GetInstance()->_pageMtx.lock();//为PageCache上锁
			PageCache::GetInstance()->ReleaseSpanToPageCache(span);//尝试合并前后页
			PageCache::GetInstance()->_pageMtx.unlock();//为PageCache解锁

			_spanLists[index]._mtx.lock();//合并完后还要再上锁，为了让当前线程走完ReleaseListToSpans函数		
		}
		start = next;
	}
	_spanLists[index]._mtx.unlock();//当前线程走完ReleaseListToSpans函数，解锁
}

