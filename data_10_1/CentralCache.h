#pragma once
#include "Common.h"

class CentralCache
{
public:
	//��ȡʵ�����õ�CnetralCache���͵ľ�̬��Ա����ĵ�ַ
	static CentralCache* GetInstance()
	{
		return &_sInst;
	}

	//ΪThreadCache����һ���������ڴ���
	size_t FetchRangeObj(void*& start, void*& end, size_t batchNum, size_t size);

	//��SpanList�л�ȡһ���ǿյ�span�����SpanListû�����ȥ��PageCache����
	Span* GetOneSpan(SpanList& list, size_t size);


	//��ThreadCache�黹�����¹���CentralCache�е�ĳ��span��
	void ReleaseListToSpans(void* start, size_t size);

private:
	SpanList _spanLists[NFREELIST];

	//����ģʽ��ʵ�ַ�ʽ�ǹ��캯���Ϳ������캯��˽�л�
	CentralCache() {}
	CentralCache(const CentralCache&) = delete;

	static CentralCache _sInst;//��̬��Ա�����ڱ���ʱ�ͻᱻ�����ڴ�
};