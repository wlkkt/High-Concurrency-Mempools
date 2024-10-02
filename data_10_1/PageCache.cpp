#include "PageCache.h"

PageCache PageCache::_sInst;

//��ȡһ���ǿ�span
Span* PageCache::NewSpan(size_t k)
{
	assert(k > 0);

	//��������span�е�ҳ������128����Ҫ�������
	if (k > NPAGES - 1)
	{
		void* ptr = SystemAlloc(k);
		_pageMtx.lock();
		Span* span = _spanPool.New();

		span->_PageId = ((size_t)ptr >> PAGE_SHIFT);
		span->_n = k;

		//_idSpanMap[span->_PageId] = span;
		_idSpanMap.set(span->_PageId, span);
		return span;
	}


	//�ȼ��PageCache�ĵ�k��Ͱ����û��span,�о�ֱ��ͷɾ������
	if (!_spanLists[k].Empty())
	{
		Span* kSpan = _spanLists[k].PopFront();
		//����ҳ�ź�span��ӳ���ϵ
		//��Ϊ����PacgeCache�е�spanֻ���������βҳ�ź���Ķ�Ӧ��ϵ������ʱ��spanҪ�������CentralCache�ˣ���CentralCacheҪ���зֳ�С�ڴ�飬С�ڴ���ThreadCache�黹ʱ��Ҫ����С�ڴ��ĵ�ַȷ������span����Ҫ������span������ҳ�ź͸�span�Ķ�Ӧ��ϵ
		for (size_t i = 0; i < kSpan->_n; ++i)
		{
			//_idSpanMap[kSpan->_PageId + i] = kSpan;
			_idSpanMap.set(kSpan->_PageId + i, kSpan);
		}
		return kSpan;
	}

	//�ߵ��������k��ͰΪ��,�������Ͱ��û�д��span������һ��
	for (size_t i = k + 1; i < NPAGES; ++i)//��Ϊ��һ��Ҫѯ�ʵĿ϶���kͰ����һ��Ͱ����i = k + 1
	{
		//kҳ��span���ظ�CentralCache,i-kҳ��span�ҵ�i-k��Ͱ�У�����Ҫ�洢ҳ�ź�span��ӳ���ϵ
		if (!_spanLists[i].Empty())
		{
			Span* nSpan = _spanLists[i].PopFront();
			Span* kSpan = _spanPool.New();

			//��nSpanͷ����һ��kҳ��span����
			kSpan->_PageId = nSpan->_PageId;
			kSpan->_n = k;

			nSpan->_PageId += k;//nSpan�������ҳҳ�ű�Ϊ��i + k
			nSpan->_n -= k;//nSpan�����ҳ����Ϊ��i - k

			_spanLists[nSpan->_n].PushFront(nSpan);//��nSpan���¹ҵ�PageCache�еĵ�nSpan->_n��Ͱ�У�����i - k��Ͱ

			//�洢nSpan����βҳ�Ÿ�nSpan��ӳ���ϵ,����PageCache�����ڴ�ʱ�ĺϲ�����
			//_idSpanMap[nSpan->_PageId] = nSpan;
			//_idSpanMap[nSpan->_PageId + nSpan->_n - 1] = nSpan;
			_idSpanMap.set(nSpan->_PageId, nSpan);
			_idSpanMap.set(nSpan->_PageId + nSpan->_n - 1, nSpan);

			//��span���Ѻ�ͽ���ҳ�ź�span��ӳ���ϵ������CentralCahe�ڻ�������ThreadCache��С���ڴ�ʱ���ҵ���ЩС�ڴ��������span
			for (size_t i = 0; i < kSpan->_n; ++i)
			{
				//_idSpanMap[kSpan->_PageId + i] = kSpan;
				_idSpanMap.set(kSpan->_PageId + i, kSpan);
			}

			return kSpan;
		}
	}

	//�ߵ������˵��PageCache��û�к��ʵ�span�ˣ���ʱ��ȥ�Ҷ�����һ������128ҳ��span
	Span* bigSpan = _spanPool.New();
	void* ptr = SystemAlloc(NPAGES - 1);//ptrָ��Ӷѷ�����ڴ�ռ����ʼ��ַ

	//������span��ҳ�ţ������ҳ����
	bigSpan->_PageId = (size_t)ptr >> PAGE_SHIFT;//ҳ�� = ��ʼ��ַ / ҳ��С
	bigSpan->_n = NPAGES - 1;

	_spanLists[bigSpan->_n].PushFront(bigSpan);
	return NewSpan(k);//���µ���һ���Լ�����ô��ʱPageCache�о���һ������kҳ��span�ˣ����Դ�PageCache��ֱ�ӷ����ˣ���forѭ���оͻ�return
	//���Դ��븴�ã���ѭ��ֻ��128�εĵݹ����ĵ���Դ��С
}

//���ڴ��ַ��ȡ��������span����ַ->ҳ��->span��
Span* PageCache::MapObjectToSpan(void* obj)
{
	size_t id = ((size_t)obj >> PAGE_SHIFT);//ҳ�� = ��ַ / ҳ��С
	//std::unique_lock<std::mutex> lc(_pageMtx);

	//auto ret = _idSpanMap.find(id);
	auto ret = _idSpanMap.get(id);
	//if (ret != _idSpanMap.end())
	//{
	//	return ret->second;
	//}
	if (ret != nullptr)
	{
		return (Span*)ret;
	}
	else
	{
		assert(false);
		return nullptr;
	}
}

//�ϲ�ҳ
void PageCache::ReleaseSpanToPageCache(Span* span)
{
	//����128ҳ��spanֱ�ӻ�����
	if (span->_n > NPAGES - 1)
	{
		void* ptr = (void*)(span->_PageId << PAGE_SHIFT);
		SystemFree(ptr);
		_spanPool.Delete(span);
		return; //��һ��return����
	}


	//��ǰ�ϲ�
	while (1)
	{
		size_t prevId = span->_PageId - 1;//��ȡǰҳ��ҳ��
		//auto ret = _idSpanMap.find(prevId);//��ҳ��ȷ���ڹ�ϣ���е�λ��
		auto ret = _idSpanMap.get(prevId);

		//ͨ��ҳ�Ų��Ҳ���span����ʾ��spanδ��PageCache�г��ֹ������ϲ�
		/*	if (ret == _idSpanMap.end())
		{
			break;
		}*/

		if (ret == nullptr)
		{
			break;
		}

		//���ҵ�span��PageCache�г��ֹ�������ʱ���ָ���CentralCache�����ϲ�
		//Span* prevSpan = ret->second;
		Span* prevSpan = (Span*)_idSpanMap.get(prevId);
		if (prevSpan->_isUse == true)
		{
			break;
		}

		//���ҵ�span��PageCache�У����͵�ǰspan�ϲ���ҳ������128�����ϲ�
		if (prevSpan->_n + span->_n > NPAGES - 1)
		{
			break;
		}

		_spanLists[prevSpan->_n].Erase(prevSpan);//��PageCache��prevSpan->_nͰ����span����ɾ��

		span->_PageId = prevSpan->_PageId;
		span->_n += prevSpan->_n;


		_spanPool.Delete(prevSpan);//��ΪNewSpan����new��һ��bigSpan��prevSpan��bigSpan��һ����
	}

	//���ϲ�
	while (1)
	{
		size_t nextId = span->_PageId + span->_n;//��ǰspan�����ҳ�ĺ�һ��span����ҳҳ��
		//auto ret = _idSpanMap.find(nextId);//��ȡҳ�Ŷ�Ӧ��Ͱλ��
		auto ret = _idSpanMap.get(nextId);
		//ͨ��ҳ�Ų��Ҳ���span����ʾ��spanδ��PageCache�г��ֹ������ϲ�
		/*if (ret == _idSpanMap.end())
		{
			break;
		}*/

		if (ret == nullptr)
		{
			break;
		}

		//���ҵ�span��PageCache�г��ֹ�������ʱ���ָ���CentralCache�����ϲ�
		//Span* nextSpan = ret->second;
		Span* nextSpan = (Span*)_idSpanMap.get(nextId);
		if (nextSpan->_isUse == true)
		{
			break;
		}

		//���ҵ�span��PageCache�У����͵�ǰspan�ϲ���ҳ������128�����ϲ�
		if (nextSpan->_n + span->_n > NPAGES - 1)
		{
			break;
		}

		span->_n += nextSpan->_n;

		_spanLists[nextSpan->_n].Erase(nextSpan);
		_spanPool.Delete(nextSpan);
	}
	_spanLists[span->_n].PushFront(span);//���ϲ����span��PageCache�й���

	//���´����βҳ��ӳ���ϵ
	//_idSpanMap[span->_PageId] = span;
	//_idSpanMap[span->_PageId + span->_n - 1] = span;
	_idSpanMap.set(span->_PageId, span); 
	_idSpanMap.set(span->_PageId + span->_n - 1, span);

	span->_isUse = false;//����ǰspan��_isUse��Ϊfalse
}


