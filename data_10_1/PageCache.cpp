#include "PageCache.h"

PageCache PageCache::_sInst;

//获取一个非空span
Span* PageCache::NewSpan(size_t k)
{
	assert(k > 0);

	//如果申请的span中的页数大于128，就要向堆申请
	if (k > NPAGES - 1)
	{
		void* ptr = SystemAlloc(k);
		_pageMtx.lock();
		Span* span = _spanPool.New();

		span->_PageId = ((size_t)ptr >> PAGE_SHIFT);
		span->_n = k;

		//_idSpanMap[span->_PageId] = span;
		_idSpanMap.set(span->_PageId, span);
		return span;
	}


	//先检查PageCache的第k个桶中有没有span,有就直接头删并返回
	if (!_spanLists[k].Empty())
	{
		Span* kSpan = _spanLists[k].PopFront();
		//保存页号和span的映射关系
		//因为留在PacgeCache中的span只存放了其首尾页号和其的对应关系，而此时该span要被分配给CentralCache了，在CentralCache要被切分成小内存块，小内存块从ThreadCache归还时需要依据小内存块的地址确定所属span，故要简历该span中所有页号和该span的对应关系
		for (size_t i = 0; i < kSpan->_n; ++i)
		{
			//_idSpanMap[kSpan->_PageId + i] = kSpan;
			_idSpanMap.set(kSpan->_PageId + i, kSpan);
		}
		return kSpan;
	}

	//走到这儿代表k号桶为空,检查后面的桶有没有大的span，分裂一下
	for (size_t i = k + 1; i < NPAGES; ++i)//因为第一个要询问的肯定是k桶的下一个桶所以i = k + 1
	{
		//k页的span返回给CentralCache,i-k页的span挂到i-k号桶中，均需要存储页号和span的映射关系
		if (!_spanLists[i].Empty())
		{
			Span* nSpan = _spanLists[i].PopFront();
			Span* kSpan = _spanPool.New();

			//在nSpan头部切一个k页的span下来
			kSpan->_PageId = nSpan->_PageId;
			kSpan->_n = k;

			nSpan->_PageId += k;//nSpan管理的首页页号变为了i + k
			nSpan->_n -= k;//nSpan管理的页数变为了i - k

			_spanLists[nSpan->_n].PushFront(nSpan);//将nSpan重新挂到PageCache中的第nSpan->_n号桶中，即第i - k号桶

			//存储nSpan的首尾页号跟nSpan的映射关系,便于PageCache回收内存时的合并查找
			//_idSpanMap[nSpan->_PageId] = nSpan;
			//_idSpanMap[nSpan->_PageId + nSpan->_n - 1] = nSpan;
			_idSpanMap.set(nSpan->_PageId, nSpan);
			_idSpanMap.set(nSpan->_PageId + nSpan->_n - 1, nSpan);

			//在span分裂后就建立页号和span得映射关系，便于CentralCahe在回收来自ThreadCache的小块内存时，找到那些小内存块所属的span
			for (size_t i = 0; i < kSpan->_n; ++i)
			{
				//_idSpanMap[kSpan->_PageId + i] = kSpan;
				_idSpanMap.set(kSpan->_PageId + i, kSpan);
			}

			return kSpan;
		}
	}

	//走到这里就说明PageCache中没有合适的span了，此时就去找堆申请一个管理128页的span
	Span* bigSpan = _spanPool.New();
	void* ptr = SystemAlloc(NPAGES - 1);//ptr指向从堆分配的内存空间的起始地址

	//计算新span的页号，管理的页数等
	bigSpan->_PageId = (size_t)ptr >> PAGE_SHIFT;//页号 = 起始地址 / 页大小
	bigSpan->_n = NPAGES - 1;

	_spanLists[bigSpan->_n].PushFront(bigSpan);
	return NewSpan(k);//重新调用一次自己，那么此时PageCache中就有一个管理k页的span了，可以从PageCache中直接分配了，在for循环中就会return
	//可以代码复用，且循环只有128次的递归消耗的资源很小
}

//由内存地址获取其所属的span（地址->页号->span）
Span* PageCache::MapObjectToSpan(void* obj)
{
	size_t id = ((size_t)obj >> PAGE_SHIFT);//页号 = 地址 / 页大小
	//std::unique_lock<std::mutex> lc(_pageMtx);

	//auto ret = _idSpanMap.find(id);
	auto ret = _idSpanMap.get(id);
	//if (ret != _idSpanMap.end())
	//{
	//	return ret->second;
	//}
	if (ret != nullptr)
	{
		return (Span*)ret;
	}
	else
	{
		assert(false);
		return nullptr;
	}
}

//合并页
void PageCache::ReleaseSpanToPageCache(Span* span)
{
	//大于128页的span直接还给堆
	if (span->_n > NPAGES - 1)
	{
		void* ptr = (void*)(span->_PageId << PAGE_SHIFT);
		SystemFree(ptr);
		_spanPool.Delete(span);
		return; //单一个return即可
	}


	//向前合并
	while (1)
	{
		size_t prevId = span->_PageId - 1;//获取前页的页号
		//auto ret = _idSpanMap.find(prevId);//由页号确定在哈希表中的位置
		auto ret = _idSpanMap.get(prevId);

		//通过页号查找不到span，表示该span未在PageCache中出现过，不合并
		/*	if (ret == _idSpanMap.end())
		{
			break;
		}*/

		if (ret == nullptr)
		{
			break;
		}

		//查找的span在PageCache中出现过，但此时被分给了CentralCache，不合并
		//Span* prevSpan = ret->second;
		Span* prevSpan = (Span*)_idSpanMap.get(prevId);
		if (prevSpan->_isUse == true)
		{
			break;
		}

		//查找的span在PageCache中，但和当前span合并后页数大于128，不合并
		if (prevSpan->_n + span->_n > NPAGES - 1)
		{
			break;
		}

		_spanLists[prevSpan->_n].Erase(prevSpan);//将PageCache中prevSpan->_n桶处的span进行删除

		span->_PageId = prevSpan->_PageId;
		span->_n += prevSpan->_n;


		_spanPool.Delete(prevSpan);//因为NewSpan中是new了一个bigSpan的prevSpan是bigSpan的一部分
	}

	//向后合并
	while (1)
	{
		size_t nextId = span->_PageId + span->_n;//当前span管理的页的后一个span的首页页号
		//auto ret = _idSpanMap.find(nextId);//获取页号对应的桶位置
		auto ret = _idSpanMap.get(nextId);
		//通过页号查找不到span，表示该span未在PageCache中出现过，不合并
		/*if (ret == _idSpanMap.end())
		{
			break;
		}*/

		if (ret == nullptr)
		{
			break;
		}

		//查找的span在PageCache中出现过，但此时被分给了CentralCache，不合并
		//Span* nextSpan = ret->second;
		Span* nextSpan = (Span*)_idSpanMap.get(nextId);
		if (nextSpan->_isUse == true)
		{
			break;
		}

		//查找的span在PageCache中，但和当前span合并后页数大于128，不合并
		if (nextSpan->_n + span->_n > NPAGES - 1)
		{
			break;
		}

		span->_n += nextSpan->_n;

		_spanLists[nextSpan->_n].Erase(nextSpan);
		_spanPool.Delete(nextSpan);
	}
	_spanLists[span->_n].PushFront(span);//将合并后的span在PageCache中挂起

	//重新存放首尾页的映射关系
	//_idSpanMap[span->_PageId] = span;
	//_idSpanMap[span->_PageId + span->_n - 1] = span;
	_idSpanMap.set(span->_PageId, span); 
	_idSpanMap.set(span->_PageId + span->_n - 1, span);

	span->_isUse = false;//将当前span的_isUse设为false
}


