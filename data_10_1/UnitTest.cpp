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

//�����ڴ���̵ĵ��ԣ����̣߳�
void TLSTest()
{
	std::thread t1(Alloc1);//����һ���µ��߳� t1������������߳���ִ�� Alloc1 ����
	std::thread t2(Alloc2);//����һ���µ��߳� t2������������߳���ִ�� Alloc2 ����

	t1.join();
	t2.join();
}
//
////�����ڴ���̵ĵ��ԣ����̣߳�
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
//	//��������һ����span
//	for (size_t i = 0; i < 1024; i++)
//	{
//		void* p1 = ConcurrentAlloc(6);
//	}
//
//	//���������һ���µ�span��ôp2ָ��ĵ�ַӦ������һ�������span�Ľ�β��ַ
//	void* p2 =  ConcurrentAlloc(8);
//	cout << p2 <<endl;
//}
//
////�ͷ��ڴ���̵ĵ��ԣ����̣߳�
//void TestConcurrentAlloc2()
//{
//
//	//���ڴ�����ʱ���������������Ϊֻ���������ܳɹ�ʹ��ĳ��span��_useCount == 0����PageCache��
//
//	//��ʼ_maxsize = 1
//	void* p1 = ConcurrentAlloc(6);//����һ����_maxsize++ == 2
//
//	void* p2 = ConcurrentAlloc(8);//����������ʱ��Ϊ_maxsize == 2,���Է���2������һʣһ,++_maxsize == 3
//	void* p3 = ConcurrentAlloc(1);//����ʣ���Ǹ�������++_maxsize
//
//	void* p4 = ConcurrentAlloc(7);//����������ʱ��Ϊ_maxsize == 3,���Է�����������һʣ����++_maxsize == 4
//	void* p5 = ConcurrentAlloc(8);//��ʣ���������������++_maxsize
//	void* p6 = ConcurrentAlloc(6);//��ʣ���������������++_maxsize
//
//	void* p7 = ConcurrentAlloc(8);//����������ʱ��Ϊ_maxsize == 4,���Է���4������һʣ����++_maxsize == 5
//	void* p8 = ConcurrentAlloc(6);//��ʣ�������������++_maxsize
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
//	//��ʱ_maxsize == 5,_freeLists[0].size == 2����ʱ���������10��8�ֽڴ�С�ڴ����span��_useCount == 10����������ʱ�����Դ�Ϊ��׼
//
//	//���մ���ʱ����Ҫ�����ͷŵĴ�С�����������ȴ���
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

//�ͷ��ڴ���̵ĵ��ԣ����̣߳�
void MultiThreadAlloc1()
{
	std::vector<void*> v;
	//ѭ���ߴα�֤�����̶߳����Խ���PageCache��
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