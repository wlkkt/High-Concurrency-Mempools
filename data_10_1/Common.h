#pragma once
#include <iostream>
#include <vector>
#include <thread>
#include <unordered_map>
#include <time.h>
#include <assert.h>
#include <Windows.h>
#include <mutex>

using std::cout;
using std::endl;

//static const 和 const static 效果相同，表示当前变量仅会在当前文件中出现

static const size_t MAX_BYTES = 256 * 1024;//规定单次向ThreadCache中申请的内存不超过256KB
static const size_t NFREELIST = 208;	   //规定ThreadCache和CentralCache中哈希桶的数量为208
static const size_t NPAGES = 129;		   //规定PageCache中span存放的最大页数为129
static const size_t PAGE_SHIFT = 13;       //规定一个页的大小为2的13次方字节，即8KB


//Windows环境下通过封装Windows提供的VirtualAlloc函数，直接向堆申请以页为单位的内存，而不使用malloc/new
inline static void* SystemAlloc(size_t kpage)//kpage表示页数
{
#ifdef _WIN32
	void* ptr = VirtualAlloc(0, kpage << PAGE_SHIFT, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#endif
	if (ptr == nullptr)
		throw std::bad_alloc();//申请失败的抛异常
	return ptr;
}

//调用Winodws提供的VirtualFree函数释放从堆申请的内存，而不使用free/delete
inline static void SystemFree(void* ptr)
{
	VirtualFree(ptr, 0, MEM_RELEASE);
}


//获取下一个结点的地址
//static限制NextObj的作用域，防止其它文件使用extern访问NextObj函数，传引用返回减少拷贝消耗
static void*& NextObj(void* obj)
{
	return *(void**)obj;
}


//管理切分好的小对象的自由链表
class FreeList
{
public:
	//头插
	void Push(void* obj)
	{
		assert(obj);
		NextObj(obj) = _freeList;
		_freeList = obj;
		++_size;
	}

	//一次性插入n个结点
	void PushRange(void* start, void* end, size_t n)
	{
		NextObj(end) = _freeList;
		_freeList = start;
		_size += n;
	}

	//头删
	void* Pop()
	{
		assert(_freeList);//当前负责释放内存结点的自由链表不能为空
		void* obj = _freeList;
		_freeList = NextObj(obj);
		--_size;
		return obj;
	}

	//头删n个内存结点（或者叫剔除，因为这些内存结点只不过是从freelist中拿出来了并未消失）
	void PopRange(void*& start, void*& end, size_t n)
	{
		assert(n <= _size);//要剔除的结点个数不能大于当前链表中结点的个数
		start = end = _freeList;

		for (size_t i = 0; i < n - 1; ++i)
		{
			end = NextObj(end);
		}
		_freeList = NextObj(end);
		NextObj(end) = nullptr;
		_size -= n;
	}

	//判空，当前自由链表是否为空
	bool Empty()
	{
		return  _freeList == nullptr;
	}

	//返回当前freelist一次性最多向CentralCache申请多少个内存结点
	size_t& MaxSize()
	{
		return _maxSize;
	}

	//返回当前链表中结点的个数
	size_t Size()
	{
		return _size;
	}

private:
	void* _freeList = nullptr;
	size_t _maxSize = 1;//记录当前freelist一次性最多向CentralCache申请多少个内存结点
	size_t _size = 0;//当前链表中结点的个数
};


//存放三种常用计算函数的类
class SizeClass
{
public:
	//基本原则：申请的内存越大，所需要的对齐数越大，整体控制在最多10%左右的内碎片浪费（如果要的内存是15byte，那么在1,128范围内按8byte对齐后的内碎片应该为1，1/16=0.0625四舍五入就是百分之十）

	//[1,128]                    按8byte对齐			freelist[0，16)     128 / 8  = 16
	//[128+1.1024]				 按16byte对齐			freelist[16，72)    896 / 16 = 56
	//[1024+1,8*1024]			 按128byte对齐			freelist[72，128)	...
	//[8*1024+1,64*1024]		 按1024byte对齐			freelist[128，184)  ...
	//[64*1024+1,256*1024]		 按8*1024byte对齐		freelist[184，208)	...


	//内联函数在程序的编译期间在使用位置展开，一般是简短且频繁使用的函数，减少函数调用产生的消耗，增加代码执行效率
	static inline size_t _RoundUp(size_t bytes, size_t alignNum)//（申请的内存大小，规定的对齐数）
	{
		size_t alignSize = 0;//对齐后的内存大小
		if (bytes % alignNum != 0)//不能按与之配对的对齐数进行对齐的，就按照与其一起传入的对齐数进行对齐计算
		{
			alignSize = (bytes / alignNum + 1) * alignNum;//bytes = 50 alignNum = 8，对齐后大小就为56
		}
		else//能按与其配对的对齐数进行对齐的，对齐后大小就是传入的申请内存大小
		{
			alignSize = bytes;//bytes = 16 alignNum = 8，对齐后大小就为16
		}
		return alignSize;
	}

	//内存对齐
	static inline size_t RoundUp(size_t size)
	{
		if (size <= 128)
		{
			return _RoundUp(size, 8);
		}
		else if (size <= 1024)
		{
			return _RoundUp(size, 16);
		}
		else if (size <= 8 * 1024)
		{
			return _RoundUp(size, 128);
		}
		else if (size <= 64 * 1024)
		{
			return _RoundUp(size, 1024);
		}
		else if (size <= 256 * 1024)
		{
			return _RoundUp(size, 8 * 1024);
		}
		else
		{
			return _RoundUp(size, 1 << PAGE_SHIFT);//用于处理大于256KB的内存申请，直接用页
		}
	}


	static inline size_t _Index(size_t bytes, size_t align_shift)
	{
		return ((bytes + (1 << align_shift) - 1) >> align_shift) - 1;
	}

	//寻找桶位置
	static inline size_t Index(size_t bytes)
	{
		assert(bytes <= MAX_BYTES);
		static int group_array[4] = { 16,56,56,56 };//提前写出计算好的每个链表的个数
		if (bytes <= 128)
		{
			return _Index(bytes, 3);
		}
		else if (bytes <= 1024)
		{
			return _Index(bytes - 128, 4) + group_array[0];//加上上一个范围内的桶的个数
		}
		else if (bytes <= 8 * 1024)
		{
			return _Index(bytes - 1024, 7) + group_array[0] + group_array[1];
		}
		else if (bytes <= 64 * 1024)
		{
			return _Index(bytes - 8 * 1024, 10) + group_array[0] + group_array[1] + group_array[2];
		}
		else if (bytes <= 256 * 1024)
		{
			return _Index(bytes - 64 * 1024, 13) + group_array[0] + group_array[1] + group_array[2] + group_array[3];
		}
		else
		{
			assert(false);
			return -1;
		}
	}

	//计算当前自由链表一次性向最多可以从CentracCache分配到的内存结点个数
	static size_t NumMoveSize(size_t size)
	{
		assert(size > 0);
		//num∈[2,512]
		int num = MAX_BYTES / size;
		if (num < 2)//最少要給2个
		{
			//num = 256KB / 512KB = 0.5 ≈ 1 个
			//num越小表示单次申请所需的内存越大，而给太少不合适
			num = 2;
		}
		if (num > 512)//最多能给512个
		{
			//num = 256KB / 50Byte ≈ 5242个
			//num越大表示单次申请所需的内存越小，而给太多不合适，会导致分配耗时太大
			num = 512;
		}
		return num;
	}


	//一次性要向堆申请多少个页
	static size_t NumMovePage(size_t size)
	{
		size_t batchnum = NumMoveSize(size);
		size_t npage = (batchnum * size) >> PAGE_SHIFT;//（最多可以分配的内存结点个数 * 单个内存结点的大小） / 每个页的大小

		if (npage == 0)//所需页数小于1，就主动给分配一个
			npage = 1;
		return npage;
	}
};


struct Span
{
	size_t _PageId = 0;//当前span管理的连续页的起始页的页号
	size_t _n = 0;//当前span管理的页的数量

	Span* _next = nullptr;
	Span* _prev = nullptr;

	size_t _objSize = 0;//当前span中切分出去的内存大小
	size_t _useCount = 0;//当前span中切好小块内存，被分配给thread cache的数量
	void* _freelist = nullptr; //管理当前span切分好的小块内存的自由链表

	bool _isUse = false;//判断当前span是否在被使用，如果没有则可以在PageCache中合成更大页
};

//管理某个桶下所有span1的数据结构（带头双向循环链表）
class SpanList
{
public:
	//构造初始的SpanList
	SpanList()
	{
		_head = new Span;
		_head->_next = _head;
		_head->_prev = _head;
	}

	//返回指向链表头结点的指针
	Span* Begin()
	{
		return _head->_next;
	}

	//返回指向链表尾结点的指针
	Span* End()
	{
		return _head;
	}

	//头插
	void PushFront(Span* span)
	{
		Insert(Begin(), span);
	}

	//在pos位置前插入
	//位置描述:prev newspan pos
	void Insert(Span* pos, Span* newSpan)
	{
		assert(pos && newSpan);
		Span* prev = pos->_prev;
		prev->_next = newSpan;
		newSpan->_prev = prev;
		newSpan->_next = pos;
		pos->_prev = newSpan;
	}

	//头删
	Span* PopFront()
	{
		Span* front = _head->_next;//_head->_next指向的是那个有用的第一个结点而不是哨兵位
		Erase(front);
		return front;//删掉后就要用，所以要返回删掉的那块内存的地址	
	}

	//删除pos位置的span
	//位置描述：prev pos next
	void Erase(Span* pos)
	{
		assert(pos && pos != _head);//指定位置不能为空且删除位置不能是头节点

		//暂存一下位置
		Span* prev = pos->_prev;
		Span* next = pos->_next;

		prev->_next = next;
		next->_prev = prev;
	}

	//判断是否为空
	bool Empty()
	{
		return _head->_next == _head;
	}

	std::mutex _mtx; //桶锁
private:
	Span* _head = nullptr;
};