#pragma once
#include "Common.h"

class CentralCache
{
public:
	//获取实例化好的CnetralCache类型的静态成员对象的地址
	static CentralCache* GetInstance()
	{
		return &_sInst;
	}

	//为ThreadCache分配一定数量的内存结点
	size_t FetchRangeObj(void*& start, void*& end, size_t batchNum, size_t size);

	//从SpanList中获取一个非空的span，如果SpanList没有则会去问PageCache申请
	Span* GetOneSpan(SpanList& list, size_t size);


	//将ThreadCache归还的重新挂在CentralCache中的某个span上
	void ReleaseListToSpans(void* start, size_t size);

private:
	SpanList _spanLists[NFREELIST];

	//单例模式的实现方式是构造函数和拷贝构造函数私有化
	CentralCache() {}
	CentralCache(const CentralCache&) = delete;

	static CentralCache _sInst;//静态成员变量在编译时就会被分配内存
};