#pragma once
#include "Common.h"


class ThreadCache
{
public:

	//分配ThreadCache中的内存
	void* Allocate(size_t bytes);

	//释放ThreadCache中的内存
	void Deallocate(void* ptr, size_t size);

	//从CentralCache中获取内存结点
	void* FetchFromCentralCache(size_t index, size_t size);

	//释放内存结点导致自由链表结点个数过多时，依据当前自由链表一次性最多向CentralCache申请的内存结点的个数，向CentralCache归还这么多的内存结点
	void ListTooLong(FreeList& list, size_t size);
private:
	FreeList _freeLists[NFREELIST];//ThreadCache的208个桶下都是自由链表
};

//TLS无锁技术
//static保证该指针只在当前文件可见防止因为多个头文件包含导致的链接时出现多个相同名称的指针
static _declspec(thread) ThreadCache* pTLSThreadCache = nullptr;