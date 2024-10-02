#pragma once
#include "Common.h"

template<class T>//模板参数T
class ObjectPool
{
public:

	//为T类型的对象构造一大块内存空间
	T* New()
	{
		T* obj = nullptr;

		if (_freelist != nullptr)
		{
			//头删
			void* next = *(void**)_freelist;//next指向自由链表的第二个结点
			obj = (T*)_freelist;
			_freelist = next;
		}
		else//自由链表没东西才会去用大块内存
		{
			//剩余内存不够一个T对象大小时，重新开大块空间
			if (_remainBytes < sizeof(T))
			{
				_remainBytes = 128 * 1024;//初始设定_remainBytes为128Kb大小，其实也是设定了每次要重新申请的大块内存的大小为128Kb
				_memory = (char*)SystemAlloc(_remainBytes >> PAGE_SHIFT);//向SystemAlloc函数传递的是要向操作系统申请的页数而不是整体的字节数（在SystemAlloc函数中会再次转换为具体字节数）
				if (_memory == nullptr)
				{
					throw std::bad_alloc();//申请失败就抛异常
				}
			}

			obj = (T*)_memory;
			size_t objsize = sizeof(T) < sizeof(void*) ? sizeof(void*) : sizeof(T);//无论T对象需要的内存大小有多大，则每次分配的内存应该大于等于当前环境下一个指针的大小，从而保证可以顺利存放下一个结点的地址
			_memory += objsize;
			_remainBytes -= objsize;
		}


		//定位new，显示调用T的构造函数初始化
		new(obj)T;
		return obj;
	}

	//回收内存
	void Delete(T* obj)//传入指向要回收的对象的指针
	{
		//显示调用析构函数清理对象
		obj->~T();

		/*可以不考虑链表是否为空的情况，直接头插即可，因为_freelist起始为空（不信自行带入测试）
		if(_freelist == nullptr)//链表为空就先头插
		{
			_freelist = obj;
			//*(int*)obj = nullptr;//淘汰
			*(void**)obj = nullptr;
		}
		else//头插
		{
			*(void**)obj = _freelist;
			_freelist = obj;
		}
		*/

		//修改后
		* (void**)obj = _freelist;
		_freelist = obj;
	}
	std::mutex _poolMtx;//用于为ObjectPool加锁
private:

	char* _memory = nullptr;//指向大块内存的指针
	size_t _remainBytes = 0;//大块内存在切分过程中剩余字节数
	void* _freelist = nullptr;//自由链表，因为借用内存的对象的类型是不确定的所以要使用void*
};