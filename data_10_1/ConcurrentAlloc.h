#pragma once
#include "ThreadCache.h"
#include "PageCache.h"

//�ֲ߳̾��洢TLS����һ�ֱ����Ĵ洢��������������������ڵ��߳����ǰ�ȫ�ɷ��ʵģ����ǲ��ܱ������̷߳��ʣ������ͱ��������ݵ��̶߳����ԡ�

//�����ڴ�
static void* ConcurrentAlloc(size_t size)
{
	if (size > MAX_BYTES)//���ε��ڴ��������256KBʱ
	{
		size_t alignSize = SizeClass::RoundUp(size);//������ҳ�Ĵ�С����
		size_t kpage = alignSize >> PAGE_SHIFT;//�õ�Ҫ�����ҳ�ĸ���

		PageCache::GetInstance()->_pageMtx.lock();
		Span* span = PageCache::GetInstance()->NewSpan(kpage);//�п�����32ҳ��128ҳ֮��,Ҳ�п��ܴ���128ҳ,����128ҳ��NewSpan��Ҫ�����⴦��
		span->_objSize = size;//��д��ȡ����span��_objSize
		PageCache::GetInstance()->_pageMtx.unlock();

		void* ptr = (void*)(span->_PageId << PAGE_SHIFT);//��ַ = ҳ�� / ҳ��С

		return ptr;//���ػ�ȡ�����ڴ�
	}
	else
	{
		//ͨ��TLS������ÿ���߳̿��������Ļ�ȡ�Լ�ר����ThreadCache����
		if (pTLSThreadCache == nullptr)
		{
			static ObjectPool<ThreadCache> tcPool;//static���α�ֻ֤�ڵ�ǰ�ļ��п��Ա�����

			tcPool._poolMtx.lock();
			pTLSThreadCache = tcPool.New();
			tcPool._poolMtx.unlock();
		}

		//Allocate����ִ��ʱ���ܻᾭ���ܶ��ļ�����GenOneSpan��NewSpan�Ȳ��ܷ��أ���������·��صĽ���������뵽���ڴ�ĵ�ַ
		return pTLSThreadCache->Allocate(size);
	}
}

//�ͷ��ڴ�
static void ConcurrentFree(void* ptr)
{
	assert(ptr);
	Span* span = PageCache::GetInstance()->MapObjectToSpan(ptr);//��Ҫ�ͷŵ��ڴ�ĵ�ַ��ȡ������ڴ��span
	size_t size = span->_objSize;
	if (size > MAX_BYTES)
	{
		PageCache::GetInstance()->_pageMtx.lock();
		PageCache::GetInstance()->ReleaseSpanToPageCache(span);//����span���¹���PacgeCache�򷵻ظ���
		PageCache::GetInstance()->_pageMtx.unlock();
	}
	else
	{
		//�������ͷ�ʱpTLSThreadCache����Ϊ��
		assert(pTLSThreadCache);
		pTLSThreadCache->Deallocate(ptr, size);
	}
}
