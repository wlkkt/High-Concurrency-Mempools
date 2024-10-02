#pragma once
#include "Common.h"


class ThreadCache
{
public:

	//����ThreadCache�е��ڴ�
	void* Allocate(size_t bytes);

	//�ͷ�ThreadCache�е��ڴ�
	void Deallocate(void* ptr, size_t size);

	//��CentralCache�л�ȡ�ڴ���
	void* FetchFromCentralCache(size_t index, size_t size);

	//�ͷ��ڴ��㵼�������������������ʱ�����ݵ�ǰ��������һ���������CentralCache������ڴ���ĸ�������CentralCache�黹��ô����ڴ���
	void ListTooLong(FreeList& list, size_t size);
private:
	FreeList _freeLists[NFREELIST];//ThreadCache��208��Ͱ�¶�����������
};

//TLS��������
//static��֤��ָ��ֻ�ڵ�ǰ�ļ��ɼ���ֹ��Ϊ���ͷ�ļ��������µ�����ʱ���ֶ����ͬ���Ƶ�ָ��
static _declspec(thread) ThreadCache* pTLSThreadCache = nullptr;