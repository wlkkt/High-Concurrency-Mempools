#pragma once
#include "Common.h"
#include "ObjectPool.h"
#include "PageMap.h"

class PageCache
{
public:
	static PageCache* GetInstance()
	{
		return &_sInst;
	}

	//获取一个管理k页的span，也许获取的是PageCache现存的span，也许是向堆新申请的span
	Span* NewSpan(size_t k);

	//获取从内存结点的地址到span的映射
	Span* MapObjectToSpan(void* obj);

	//接收CentralCache中归还的span，并尝试合并该span的相邻空闲页，使得该span变成一个管理更多页的span
	void ReleaseSpanToPageCache(Span* span);

	std::mutex _pageMtx;//pagecache不能用桶锁,只能用全局锁,因为后面可能会有span的合并和分裂
private:
	SpanList _spanLists[NPAGES];
	ObjectPool<Span> _spanPool;//为了调用ObjectPool中的New

	//std::unordered_map<size_t, Span*> _idSpanMap;//存放页号和span的映射关系
	TCMalloc_PageMap1<32 - PAGE_SHIFT> _idSpanMap;

	PageCache() {}
	PageCache(const PageCache&) = delete;

	static PageCache _sInst;
};