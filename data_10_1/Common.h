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

//static const �� const static Ч����ͬ����ʾ��ǰ���������ڵ�ǰ�ļ��г���

static const size_t MAX_BYTES = 256 * 1024;//�涨������ThreadCache��������ڴ治����256KB
static const size_t NFREELIST = 208;	   //�涨ThreadCache��CentralCache�й�ϣͰ������Ϊ208
static const size_t NPAGES = 129;		   //�涨PageCache��span��ŵ����ҳ��Ϊ129
static const size_t PAGE_SHIFT = 13;       //�涨һ��ҳ�Ĵ�СΪ2��13�η��ֽڣ���8KB


//Windows������ͨ����װWindows�ṩ��VirtualAlloc������ֱ�����������ҳΪ��λ���ڴ棬����ʹ��malloc/new
inline static void* SystemAlloc(size_t kpage)//kpage��ʾҳ��
{
#ifdef _WIN32
	void* ptr = VirtualAlloc(0, kpage << PAGE_SHIFT, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#endif
	if (ptr == nullptr)
		throw std::bad_alloc();//����ʧ�ܵ����쳣
	return ptr;
}

//����Winodws�ṩ��VirtualFree�����ͷŴӶ�������ڴ棬����ʹ��free/delete
inline static void SystemFree(void* ptr)
{
	VirtualFree(ptr, 0, MEM_RELEASE);
}


//��ȡ��һ�����ĵ�ַ
//static����NextObj�������򣬷�ֹ�����ļ�ʹ��extern����NextObj�����������÷��ؼ��ٿ�������
static void*& NextObj(void* obj)
{
	return *(void**)obj;
}


//�����зֺõ�С�������������
class FreeList
{
public:
	//ͷ��
	void Push(void* obj)
	{
		assert(obj);
		NextObj(obj) = _freeList;
		_freeList = obj;
		++_size;
	}

	//һ���Բ���n�����
	void PushRange(void* start, void* end, size_t n)
	{
		NextObj(end) = _freeList;
		_freeList = start;
		_size += n;
	}

	//ͷɾ
	void* Pop()
	{
		assert(_freeList);//��ǰ�����ͷ��ڴ��������������Ϊ��
		void* obj = _freeList;
		_freeList = NextObj(obj);
		--_size;
		return obj;
	}

	//ͷɾn���ڴ��㣨���߽��޳�����Ϊ��Щ�ڴ���ֻ�����Ǵ�freelist���ó����˲�δ��ʧ��
	void PopRange(void*& start, void*& end, size_t n)
	{
		assert(n <= _size);//Ҫ�޳��Ľ��������ܴ��ڵ�ǰ�����н��ĸ���
		start = end = _freeList;

		for (size_t i = 0; i < n - 1; ++i)
		{
			end = NextObj(end);
		}
		_freeList = NextObj(end);
		NextObj(end) = nullptr;
		_size -= n;
	}

	//�пգ���ǰ���������Ƿ�Ϊ��
	bool Empty()
	{
		return  _freeList == nullptr;
	}

	//���ص�ǰfreelistһ���������CentralCache������ٸ��ڴ���
	size_t& MaxSize()
	{
		return _maxSize;
	}

	//���ص�ǰ�����н��ĸ���
	size_t Size()
	{
		return _size;
	}

private:
	void* _freeList = nullptr;
	size_t _maxSize = 1;//��¼��ǰfreelistһ���������CentralCache������ٸ��ڴ���
	size_t _size = 0;//��ǰ�����н��ĸ���
};


//������ֳ��ü��㺯������
class SizeClass
{
public:
	//����ԭ��������ڴ�Խ������Ҫ�Ķ�����Խ��������������10%���ҵ�����Ƭ�˷ѣ����Ҫ���ڴ���15byte����ô��1,128��Χ�ڰ�8byte����������ƬӦ��Ϊ1��1/16=0.0625����������ǰٷ�֮ʮ��

	//[1,128]                    ��8byte����			freelist[0��16)     128 / 8  = 16
	//[128+1.1024]				 ��16byte����			freelist[16��72)    896 / 16 = 56
	//[1024+1,8*1024]			 ��128byte����			freelist[72��128)	...
	//[8*1024+1,64*1024]		 ��1024byte����			freelist[128��184)  ...
	//[64*1024+1,256*1024]		 ��8*1024byte����		freelist[184��208)	...


	//���������ڳ���ı����ڼ���ʹ��λ��չ����һ���Ǽ����Ƶ��ʹ�õĺ��������ٺ������ò��������ģ����Ӵ���ִ��Ч��
	static inline size_t _RoundUp(size_t bytes, size_t alignNum)//��������ڴ��С���涨�Ķ�������
	{
		size_t alignSize = 0;//�������ڴ��С
		if (bytes % alignNum != 0)//���ܰ���֮��ԵĶ��������ж���ģ��Ͱ�������һ����Ķ��������ж������
		{
			alignSize = (bytes / alignNum + 1) * alignNum;//bytes = 50 alignNum = 8��������С��Ϊ56
		}
		else//�ܰ�������ԵĶ��������ж���ģ�������С���Ǵ���������ڴ��С
		{
			alignSize = bytes;//bytes = 16 alignNum = 8��������С��Ϊ16
		}
		return alignSize;
	}

	//�ڴ����
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
			return _RoundUp(size, 1 << PAGE_SHIFT);//���ڴ������256KB���ڴ����룬ֱ����ҳ
		}
	}


	static inline size_t _Index(size_t bytes, size_t align_shift)
	{
		return ((bytes + (1 << align_shift) - 1) >> align_shift) - 1;
	}

	//Ѱ��Ͱλ��
	static inline size_t Index(size_t bytes)
	{
		assert(bytes <= MAX_BYTES);
		static int group_array[4] = { 16,56,56,56 };//��ǰд������õ�ÿ������ĸ���
		if (bytes <= 128)
		{
			return _Index(bytes, 3);
		}
		else if (bytes <= 1024)
		{
			return _Index(bytes - 128, 4) + group_array[0];//������һ����Χ�ڵ�Ͱ�ĸ���
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

	//���㵱ǰ��������һ�����������Դ�CentracCache���䵽���ڴ������
	static size_t NumMoveSize(size_t size)
	{
		assert(size > 0);
		//num��[2,512]
		int num = MAX_BYTES / size;
		if (num < 2)//����Ҫ�o2��
		{
			//num = 256KB / 512KB = 0.5 �� 1 ��
			//numԽС��ʾ��������������ڴ�Խ�󣬶���̫�ٲ�����
			num = 2;
		}
		if (num > 512)//����ܸ�512��
		{
			//num = 256KB / 50Byte �� 5242��
			//numԽ���ʾ��������������ڴ�ԽС������̫�಻���ʣ��ᵼ�·����ʱ̫��
			num = 512;
		}
		return num;
	}


	//һ����Ҫ���������ٸ�ҳ
	static size_t NumMovePage(size_t size)
	{
		size_t batchnum = NumMoveSize(size);
		size_t npage = (batchnum * size) >> PAGE_SHIFT;//�������Է�����ڴ������ * �����ڴ���Ĵ�С�� / ÿ��ҳ�Ĵ�С

		if (npage == 0)//����ҳ��С��1��������������һ��
			npage = 1;
		return npage;
	}
};


struct Span
{
	size_t _PageId = 0;//��ǰspan���������ҳ����ʼҳ��ҳ��
	size_t _n = 0;//��ǰspan�����ҳ������

	Span* _next = nullptr;
	Span* _prev = nullptr;

	size_t _objSize = 0;//��ǰspan���зֳ�ȥ���ڴ��С
	size_t _useCount = 0;//��ǰspan���к�С���ڴ棬�������thread cache������
	void* _freelist = nullptr; //����ǰspan�зֺõ�С���ڴ����������

	bool _isUse = false;//�жϵ�ǰspan�Ƿ��ڱ�ʹ�ã����û���������PageCache�кϳɸ���ҳ
};

//����ĳ��Ͱ������span1�����ݽṹ����ͷ˫��ѭ������
class SpanList
{
public:
	//�����ʼ��SpanList
	SpanList()
	{
		_head = new Span;
		_head->_next = _head;
		_head->_prev = _head;
	}

	//����ָ������ͷ����ָ��
	Span* Begin()
	{
		return _head->_next;
	}

	//����ָ������β����ָ��
	Span* End()
	{
		return _head;
	}

	//ͷ��
	void PushFront(Span* span)
	{
		Insert(Begin(), span);
	}

	//��posλ��ǰ����
	//λ������:prev newspan pos
	void Insert(Span* pos, Span* newSpan)
	{
		assert(pos && newSpan);
		Span* prev = pos->_prev;
		prev->_next = newSpan;
		newSpan->_prev = prev;
		newSpan->_next = pos;
		pos->_prev = newSpan;
	}

	//ͷɾ
	Span* PopFront()
	{
		Span* front = _head->_next;//_head->_nextָ������Ǹ����õĵ�һ�����������ڱ�λ
		Erase(front);
		return front;//ɾ�����Ҫ�ã�����Ҫ����ɾ�����ǿ��ڴ�ĵ�ַ	
	}

	//ɾ��posλ�õ�span
	//λ��������prev pos next
	void Erase(Span* pos)
	{
		assert(pos && pos != _head);//ָ��λ�ò���Ϊ����ɾ��λ�ò�����ͷ�ڵ�

		//�ݴ�һ��λ��
		Span* prev = pos->_prev;
		Span* next = pos->_next;

		prev->_next = next;
		next->_prev = prev;
	}

	//�ж��Ƿ�Ϊ��
	bool Empty()
	{
		return _head->_next == _head;
	}

	std::mutex _mtx; //Ͱ��
private:
	Span* _head = nullptr;
};