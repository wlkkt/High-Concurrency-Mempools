#pragma once
#include "Common.h"
#include "ObjectPool.h"
#include "PageMap.h"

class PageCache
{
public:
	static PageCache* GetInstance()
	{
		return &_sInst;
	}

	//��ȡһ������kҳ��span��Ҳ���ȡ����PageCache�ִ��span��Ҳ��������������span
	Span* NewSpan(size_t k);

	//��ȡ���ڴ���ĵ�ַ��span��ӳ��
	Span* MapObjectToSpan(void* obj);

	//����CentralCache�й黹��span�������Ժϲ���span�����ڿ���ҳ��ʹ�ø�span���һ���������ҳ��span
	void ReleaseSpanToPageCache(Span* span);

	std::mutex _pageMtx;//pagecache������Ͱ��,ֻ����ȫ����,��Ϊ������ܻ���span�ĺϲ��ͷ���
private:
	SpanList _spanLists[NPAGES];
	ObjectPool<Span> _spanPool;//Ϊ�˵���ObjectPool�е�New

	//std::unordered_map<size_t, Span*> _idSpanMap;//���ҳ�ź�span��ӳ���ϵ
	TCMalloc_PageMap1<32 - PAGE_SHIFT> _idSpanMap;

	PageCache() {}
	PageCache(const PageCache&) = delete;

	static PageCache _sInst;
};