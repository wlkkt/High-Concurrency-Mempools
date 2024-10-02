#include "ThreadCache.h"
#include "CentralCache.h"

//����ThreadCache�е��ڴ�
void* ThreadCache::Allocate(size_t size)
{
	//��Χ
	assert(size <= MAX_BYTES);
	size_t allignSize = SizeClass::RoundUp(size);//��ȡ�����Ĵ�С
	size_t index = SizeClass::Index(size);//ȷ��Ͱ��λ��

	if (!_freeLists[index].Empty())
	{
		return _freeLists[index].Pop();//ͷɾ����λ�õ�Ͱ���ڴ�飬��ʾ�ͷų�ȥһ�����ʹ�õ��ڴ�
	}
	else//ThreadCache��û�к��ʵ��ڴ�ռ�
	{
		return FetchFromCentralCache(index, allignSize);//��CentralCache����ͬλ�ô������ڴ�ռ�
	}
}


//��CentralCache�����ڴ�ռ�
void* ThreadCache::FetchFromCentralCache(size_t index, size_t size)
{
	//�������㷨
	size_t batchNum = min(_freeLists[index].MaxSize(), SizeClass::NumMoveSize(size));//bathcNum �� [2,512]

	if (_freeLists[index].MaxSize() == batchNum)
	{
		_freeLists[index].MaxSize() += 1;
	}

	//�����������������㷨�õ��ĵ�ǰ������һ����Ҫ��CentralCache����Ľ�����
	//�����Ǽ���ʵ��һ���Կɴ�CentralCache�����뵽�Ľ�����

	//����Ͳ���������FetchRangeObj�����������ǵ����ã����start��end�������
	void* start = nullptr;
	void* end = nullptr;

	//actualNum��ʾʵ���Ͽ��Դ�CentralCache�л�ȡ�����ڴ���ĸ���
	size_t actualNum = CentralCache::GetInstance()->FetchRangeObj(start, end, batchNum, size);
	assert(actualNum >= 1);//actualNum�ض�����ڵ���1��������Ϊ0����ΪFetchRangeObj����GetOneSpan����
	if (actualNum == 1)
	{
		assert(start == end);//��ʱstart��endӦ�ö�ָ��ý��
		return start;//ֱ�ӷ���startָ��Ľ�㼴��
	}
	else
	{
		//�����CentralCache�л�ȡ�˶���ڴ���,�򽫵�һ�����ظ�ThreadCache,Ȼ���ٽ�ʣ����ڴ����ThreadCache������������
		_freeLists[index].PushRange(NextObj(start), end, actualNum - 1);
		return start;
	}
}


//�ͷ�ThreadCache�е��ڴ�
void ThreadCache::Deallocate(void* ptr, size_t size)
{
	assert(ptr);
	assert(size <= MAX_BYTES);

	//�Ҷ�ӳ�����������Ͱ����������Ķ�������ȥ
	size_t index = SizeClass::Index(size);
	_freeLists[index].Push(ptr);

	//����������ǰ����������ڴ���������ڵ�ǰ����һ���Կ�����CentralCache������ڴ������_maxsize���ͽ���ǰ����������_maxsize���ڴ���黹��CentralCache
	//���ֻ�黹�������һС�����ڴ��㣨_size-_maxsize���ᵼ��ThreadCache��CentralCache����Ƶ���Ľ���������ϵͳ���ú��������Ĵ������Ӷ�������������
	if (_freeLists[index].Size() >= _freeLists[index].MaxSize())
	{
		ListTooLong(_freeLists[index], size);
	}

}

//���������
void ThreadCache::ListTooLong(FreeList& list, size_t size)
{
	//����Ͳ���
	void* start = nullptr;
	void* end = nullptr;

	list.PopRange(start, end, list.MaxSize());

	CentralCache::GetInstance()->ReleaseListToSpans(start, size);
}