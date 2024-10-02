#include "ObjectPool.h"
#include "ConcurrentAlloc.h"

void Alloc1()
{
	for (size_t i = 0; i < 5; ++i)
	{
		void* ptr = ConcurrentAlloc(6);
	}
}

void Alloc2()
{
	for (size_t i = 0; i < 5; ++i)
	{
		void* ptr = ConcurrentAlloc(7);
	}
}

//申请内存过程的调试（多线程）
void TLSTest()
{
	std::thread t1(Alloc1);//创建一个新的线程 t1，并且在这个线程中执行 Alloc1 函数
	std::thread t2(Alloc2);//创建一个新的线程 t2，并且在这个线程中执行 Alloc2 函数

	t1.join();
	t2.join();
}
//
////申请内存过程的调试（单线程）
//void TestConcurrentAlloc()
//{
//	//void* p1 = ConcurrentAlloc(6);
//	//void* p2 = ConcurrentAlloc(8);
//	//void* p3 = ConcurrentAlloc(1);
//	//void* p4 = ConcurrentAlloc(7);
//	//void* p5 = ConcurrentAlloc(8);
//
//	//cout << p1 << endl;
//	//cout << p2 << endl;
//	//cout << p3 << endl;
//	//cout << p4 << endl;
//	//cout << p5 << endl;
//	
//	//尝试用完一整个span
//	for (size_t i = 0; i < 1024; i++)
//	{
//		void* p1 = ConcurrentAlloc(6);
//	}
//
//	//如果用完了一个新的span那么p2指向的地址应该是上一个用完的span的结尾地址
//	void* p2 =  ConcurrentAlloc(8);
//	cout << p2 <<endl;
//}
//
////释放内存过程的调试（单线程）
//void TestConcurrentAlloc2()
//{
//
//	//比内存申请时多加两个申请是因为只有这样才能成功使得某个span的_useCount == 0进入PageCache中
//
//	//初始_maxsize = 1
//	void* p1 = ConcurrentAlloc(6);//分配一个，_maxsize++ == 2
//
//	void* p2 = ConcurrentAlloc(8);//不够再申请时因为_maxsize == 2,所以分配2个，用一剩一,++_maxsize == 3
//	void* p3 = ConcurrentAlloc(1);//用了剩的那个，不用++_maxsize
//
//	void* p4 = ConcurrentAlloc(7);//不够再申请时因为_maxsize == 3,所以分配三个，用一剩二，++_maxsize == 4
//	void* p5 = ConcurrentAlloc(8);//用剩余的那两个，不用++_maxsize
//	void* p6 = ConcurrentAlloc(6);//用剩余的那两个，不用++_maxsize
//
//	void* p7 = ConcurrentAlloc(8);//不够再申请时因为_maxsize == 4,所以分配4个，用一剩三，++_maxsize == 5
//	void* p8 = ConcurrentAlloc(6);//用剩余的三个，不用++_maxsize
//
//	cout << p1 << endl;
//	cout << p2 << endl;
//	cout << p3 << endl;
//	cout << p4 << endl;
//	cout << p5 << endl;
//	cout << p6 << endl;
//	cout << p7 << endl;
//	cout << p8 << endl;
//
//	//此时_maxsize == 5,_freeLists[0].size == 2，此时负责分配这10个8字节大小内存结点的span的_useCount == 10，后续调试时可以以此为标准
//
//	//最终代码时不需要传入释放的大小，这里我们先传入
//	ConcurrentFree(p1, 6);
//	ConcurrentFree(p2, 8);
//	ConcurrentFree(p3, 1);
//	ConcurrentFree(p4, 7);
//	ConcurrentFree(p5, 8);
//	ConcurrentFree(p6, 8);
//	ConcurrentFree(p7, 8);
//	ConcurrentFree(p8, 8);
//}
//

//释放内存过程的调试（多线程）
void MultiThreadAlloc1()
{
	std::vector<void*> v;
	//循环七次保证两个线程都可以进入PageCache中
	for (size_t i = 0; i < 7; ++i)
	{
		void* ptr = ConcurrentAlloc(6);
		v.push_back(ptr);
	}

	for (auto e : v)
	{
		ConcurrentFree(e);
	}
}

void MultiThreadAlloc2()
{
	std::vector<void*> v;
	for (size_t i = 0; i < 7; ++i)
	{
		void* ptr = ConcurrentAlloc(500);
		v.push_back(ptr);
	}

	for (auto e : v)
	{
		ConcurrentFree(e);
	}
}


void TestMultiThread()
{
	std::thread t1(MultiThreadAlloc1);
	std::thread t2(MultiThreadAlloc2);
	t1.join();
	t2.join();
}

void BigAlloc()
{
	void* p1 = ConcurrentAlloc(257 * 1024);
	ConcurrentFree(p1);

	void* p2 = ConcurrentAlloc(129 * 8 * 1024);
	ConcurrentFree(p2);
}

void WithNoSize()
{
	//void* p1 = ConcurrentAlloc(6);
	//void* p2 = ConcurrentAlloc(8);
	//void* p3 = ConcurrentAlloc(1);
	//void* p4 = ConcurrentAlloc(7);
 //
	//ConcurrentFree(p1);
	//ConcurrentFree(p2);
	//ConcurrentFree(p3);
	//ConcurrentFree(p4);

	void* p1 = ConcurrentAlloc(257 * 1024);
	ConcurrentFree(p1);

	void* p2 = ConcurrentAlloc(129 * 8 * 1024);
	ConcurrentFree(p2);

	void* p3 = ConcurrentAlloc(16);
	ConcurrentFree(p3);

}


//int main()
//{
//	//TLSTest();
//	//TestConcurrentAlloc()
//	//TestConcurrentAlloc2();
//	TestMultiThread();
//	//BigAlloc();
//	//WithNoSize();
//	return 0;
//}