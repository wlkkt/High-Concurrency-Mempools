#pragma once
#include "ThreadCache.h"
#include "PageCache.h"

//线程局部存储TLS：是一种变量的存储方法，这个变量在它所在的线程内是安全可访问的，但是不能被其它线程访问，这样就保持了数据的线程独立性。

//申请内存
static void* ConcurrentAlloc(size_t size)
{
	if (size > MAX_BYTES)//单次的内存申请大于256KB时
	{
		size_t alignSize = SizeClass::RoundUp(size);//按单个页的大小对齐
		size_t kpage = alignSize >> PAGE_SHIFT;//得到要分配的页的个数

		PageCache::GetInstance()->_pageMtx.lock();
		Span* span = PageCache::GetInstance()->NewSpan(kpage);//有可能在32页到128页之间,也有可能大于128页,大于128页在NewSpan中要做特殊处理
		span->_objSize = size;//填写获取到的span的_objSize
		PageCache::GetInstance()->_pageMtx.unlock();

		void* ptr = (void*)(span->_PageId << PAGE_SHIFT);//地址 = 页号 / 页大小

		return ptr;//返回获取到的内存
	}
	else
	{
		//通过TLS方法，每个线程可以无锁的获取自己专属的ThreadCache对象
		if (pTLSThreadCache == nullptr)
		{
			static ObjectPool<ThreadCache> tcPool;//static修饰保证只在当前文件中可以被访问

			tcPool._poolMtx.lock();
			pTLSThreadCache = tcPool.New();
			tcPool._poolMtx.unlock();
		}

		//Allocate函数执行时可能会经历很多文件比如GenOneSpan，NewSpan等才能返回，正常情况下返回的结果就是申请到的内存的地址
		return pTLSThreadCache->Allocate(size);
	}
}

//释放内存
static void ConcurrentFree(void* ptr)
{
	assert(ptr);
	Span* span = PageCache::GetInstance()->MapObjectToSpan(ptr);//由要释放的内存的地址获取管理该内存的span
	size_t size = span->_objSize;
	if (size > MAX_BYTES)
	{
		PageCache::GetInstance()->_pageMtx.lock();
		PageCache::GetInstance()->ReleaseSpanToPageCache(span);//将该span重新挂在PacgeCache或返回给堆
		PageCache::GetInstance()->_pageMtx.unlock();
	}
	else
	{
		//理论上释放时pTLSThreadCache不会为空
		assert(pTLSThreadCache);
		pTLSThreadCache->Deallocate(ptr, size);
	}
}
