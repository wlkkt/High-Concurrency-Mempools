#include "CentralCache.h"
#include "PageCache.h"

//����
CentralCache CentralCache::_sInst;

//ʵ�ʿ��Դ�CentralCache�л�ȡ�����ڴ���ĸ���
size_t CentralCache::FetchRangeObj(void*& start, void*& end, size_t batchNum, size_t size)
{
	size_t index = SizeClass::Index(size);
	_spanLists[index]._mtx.lock();//��Ͱ��
	Span* span = GetOneSpan(_spanLists[index], size);//��ȡһ���ǿ�span

	assert(span);
	assert(span->_freelist);//spanΪ�ջ���span����Ŀռ�Ϊ�վ�����

//���Դ�span�л�ȡbatchNum������,��û����ô�����Ļ�,�ж��پ͸�����
	start = end = span->_freelist;
	size_t actualNum = 1;//�Ѿ��жϹ�����������Ϊ�գ����Կ϶���һ��

	size_t i = 0;
	//NextObj(end) != nullptr���ڷ�ֹactualNumС��bathcNum��ѭ����������ʱNexeObj(end)�е�endΪnullptr�����µı���
	while (i < batchNum - 1 && NextObj(end) != nullptr)
	{
		end = NextObj(end);
		++i;
		++actualNum;
	}
	span->_useCount += actualNum;//��ǰspan����actualNum���ڴ��㱻�����ThreadCache

	//���µ�ǰspan������������
	span->_freelist = NextObj(end);
	NextObj(end) = nullptr;

	_spanLists[index]._mtx.unlock();//��Ͱ��
	return actualNum;
}


//��ȡ�ǿ�span
Span* CentralCache::GetOneSpan(SpanList& list, size_t size)
{
	//����CentralCache��ǰͰ�е�SpanListѰ��һ���ǿյ�span
	Span* it = list.Begin();
	while (it != list.End())
	{
		if (it->_freelist != nullptr)
		{
			return it;
		}
		else
		{
			it = it->_next;
		}
	}

	//�Ȱѽ���GetOneSpanǰ���õ�Ͱ����������������߳��ͷ��ڴ�ʱ������
	list._mtx.unlock();

	size_t k = SizeClass::NumMovePage(size);//SizeClass::NumMovePage(size)����Ҫ��PageCache����������ҳ��span����k

	//�ߵ�����֤��CentralCache��ǰͰ�е�SpanList��û�зǿյ�span�����ˣ���Ҫ��PageCache����
	PageCache::GetInstance()->_pageMtx.lock();//ΪPageCache��������
	Span* span = PageCache::GetInstance()->NewSpan(k);
	PageCache::GetInstance()->_pageMtx.unlock();//ΪPageCache�������

	span->_isUse = true;//�޸Ĵ�PageCache��ȡ����span��״̬Ϊ����ʹ��
	span->_objSize = size;//����spanҪ���зֳ�ȥ���ڴ��С

	//��PageCache�л�ȡ��span��û�н����ڴ��зֵģ���Ҫ�����зֲ�����������������

	//1������span�����µĴ���ڴ����ʼ�ͽ�β��ַ
	//��ʼ��ַ = ҳ�� * ҳ�Ĵ�С


	char* start = (char*)(span->_PageId << PAGE_SHIFT);//ѡ��char*������void*��Ϊ�˺���+=size��ʱ�ƶ�size���ֽ�
	//����span->_PageId = 5��PAGE_SHIFT = 13��5 >> 13 = 40960���ֽڣ�
	//����ֵ 40960 ��ʾ�ڴ��е�һ����ַλ�ã�ͨ�� (char*) ��ʾ����ת����start ��ָ��������ڴ��ַ����span����ʼ��ַ

	char* end = (char*)(start + (span->_n << PAGE_SHIFT));//endָ��span�Ľ�����ַ��span������ڴ��С = span��ҳ�ĸ��� * ҳ��С 

	//2����start��endָ��Ĵ���ڴ��гɶ��С���ڴ棬��β�������������У�����β�壬ʹ�ü�ʹ���и����������Ϊ�����ռ䣬�ӿ�����ٶȣ�

	//����������һ����Ϊͷ��㣬����β��
	span->_freelist = start;
	void* tail = start;
	start += size;

	//ѭ��β��
	while (start < end)
	{
		NextObj(tail) = start;//��ǰtailָ����ڴ���ǰ4/8���ֽڴ����һ��������ʼ��ַ����startָ��Ľ��ĵ�ַ
		start += size;//����start
		tail = NextObj(tail);//����tail
	}

	NextObj(tail) = nullptr;

	//��CentralCache�е�ǰ��SpanListͷ��ǰҪ��������ֹ�����߳�ͬʱ���ʵ�ǰ��SpanList
	list._mtx.lock();
	list.PushFront(span);

	return span;//��ʱ��span�Ѿ�������CentralCache��ĳ��Ͱ��SpanList���ˣ����ظ�span�ĵ�ַ����
}

//����ThreadCache����ڴ���黹������������span����Ϊ��Щ�ڴ����������CentralCacheͬһͰ�еĲ�ͬspan�����
//ǰһ��span�����ˣ��Ӻ�һ��span�л�ȡ��ͬʱǰһ��span���ܻ�����մ�ThreadCache�黹�������ڴ��㣬�´η���ʱ�����ֿ��Դ�ǰ���span�����ˣ����߳̿��ǵ��е��
void CentralCache::ReleaseListToSpans(void* start, size_t size)
{
	size_t index = SizeClass::Index(size);
	_spanLists[index]._mtx.lock();

	//startָ�����һ���ڴ����ͷ���
	while (start)
	{
		void* next = NextObj(start);
		Span* span = PageCache::GetInstance()->MapObjectToSpan(start);//ȷ��Ҫ�黹��span

		//ͷ���ڴ��㵽span��
		NextObj(start) = span->_freelist;
		span->_freelist = start;

		span->_useCount--;//_useCount--��ʾ��һ�������ȥ��С���ڴ�ص���ǰspan

		//��ǰspan��_useCountΪ0��ʾ��ǰspan�зֳ�ȥ������С���ڴ涼�����ˣ�ֱ�ӽ�����span����PageCache,PageCache�ٳ��Խ���ǰ��ҳ�ĺϲ�
		if (span->_useCount == 0)
		{
			_spanLists[index].Erase(span);//����ǰ��span��CentralCache��ĳ��Ͱ����SpanList��ȡ��

			//���뵽PageCache�н��кϲ���span����Ҫ������������ݣ��ÿռ���
			span->_freelist = nullptr;
			span->_next = nullptr;
			span->_prev = nullptr;

			_spanLists[index]._mtx.unlock();//�����˾ͽ���,�����CentralCache��ͬһͰ��������

			PageCache::GetInstance()->_pageMtx.lock();//ΪPageCache����
			PageCache::GetInstance()->ReleaseSpanToPageCache(span);//���Ժϲ�ǰ��ҳ
			PageCache::GetInstance()->_pageMtx.unlock();//ΪPageCache����

			_spanLists[index]._mtx.lock();//�ϲ����Ҫ��������Ϊ���õ�ǰ�߳�����ReleaseListToSpans����		
		}
		start = next;
	}
	_spanLists[index]._mtx.unlock();//��ǰ�߳�����ReleaseListToSpans����������
}

