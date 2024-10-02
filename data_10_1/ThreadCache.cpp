#include "ThreadCache.h"
#include "CentralCache.h"

//分配ThreadCache中的内存
void* ThreadCache::Allocate(size_t size)
{
	//范围
	assert(size <= MAX_BYTES);
	size_t allignSize = SizeClass::RoundUp(size);//获取对齐后的大小
	size_t index = SizeClass::Index(size);//确认桶的位置

	if (!_freeLists[index].Empty())
	{
		return _freeLists[index].Pop();//头删符合位置的桶的内存块，表示释放出去一块可以使用的内存
	}
	else//ThreadCache中没有合适的内存空间
	{
		return FetchFromCentralCache(index, allignSize);//向CentralCache的相同位置处申请内存空间
	}
}


//向CentralCache申请内存空间
void* ThreadCache::FetchFromCentralCache(size_t index, size_t size)
{
	//慢调节算法
	size_t batchNum = min(_freeLists[index].MaxSize(), SizeClass::NumMoveSize(size));//bathcNum ∈ [2,512]

	if (_freeLists[index].MaxSize() == batchNum)
	{
		_freeLists[index].MaxSize() += 1;
	}

	//上述部分是满调节算法得到的当前理论上一次性要向CentralCache申请的结点个数
	//下面是计算实际一次性可从CentralCache中申请到的结点个数

	//输出型参数，传入FetchRangeObj函数的是它们的引用，会对start和end进行填充
	void* start = nullptr;
	void* end = nullptr;

	//actualNum表示实际上可以从CentralCache中获取到的内存结点的个数
	size_t actualNum = CentralCache::GetInstance()->FetchRangeObj(start, end, batchNum, size);
	assert(actualNum >= 1);//actualNum必定会大于等于1，不可能为0，因为FetchRangeObj还有GetOneSpan函数
	if (actualNum == 1)
	{
		assert(start == end);//此时start和end应该都指向该结点
		return start;//直接返回start指向的结点即可
	}
	else
	{
		//如果从CentralCache中获取了多个内存结点,则将第一个返回给ThreadCache,然后再将剩余的内存挂在ThreadCache的自由链表中
		_freeLists[index].PushRange(NextObj(start), end, actualNum - 1);
		return start;
	}
}


//释放ThreadCache中的内存
void ThreadCache::Deallocate(void* ptr, size_t size)
{
	assert(ptr);
	assert(size <= MAX_BYTES);

	//找对映射的自由链表桶，并将用完的对象插入进去
	size_t index = SizeClass::Index(size);
	_freeLists[index].Push(ptr);

	//当还回来后当前自由链表的内存结点个数大于当前链表一次性可以向CentralCache申请的内存结点个数_maxsize，就将当前自由链表中_maxsize个内存结点归还给CentralCache
	//如果只归还多出的那一小部分内存结点（_size-_maxsize）会导致ThreadCache和CentralCache进行频繁的交互，增加系统调用和锁竞争的次数，从而降低整体性能
	if (_freeLists[index].Size() >= _freeLists[index].MaxSize())
	{
		ListTooLong(_freeLists[index], size);
	}

}

//链表结点过多
void ThreadCache::ListTooLong(FreeList& list, size_t size)
{
	//输出型参数
	void* start = nullptr;
	void* end = nullptr;

	list.PopRange(start, end, list.MaxSize());

	CentralCache::GetInstance()->ReleaseListToSpans(start, size);
}