#pragma once
#include "Common.h"

template<class T>//ģ�����T
class ObjectPool
{
public:

	//ΪT���͵Ķ�����һ����ڴ�ռ�
	T* New()
	{
		T* obj = nullptr;

		if (_freelist != nullptr)
		{
			//ͷɾ
			void* next = *(void**)_freelist;//nextָ����������ĵڶ������
			obj = (T*)_freelist;
			_freelist = next;
		}
		else//��������û�����Ż�ȥ�ô���ڴ�
		{
			//ʣ���ڴ治��һ��T�����Сʱ�����¿����ռ�
			if (_remainBytes < sizeof(T))
			{
				_remainBytes = 128 * 1024;//��ʼ�趨_remainBytesΪ128Kb��С����ʵҲ���趨��ÿ��Ҫ��������Ĵ���ڴ�Ĵ�СΪ128Kb
				_memory = (char*)SystemAlloc(_remainBytes >> PAGE_SHIFT);//��SystemAlloc�������ݵ���Ҫ�����ϵͳ�����ҳ��������������ֽ�������SystemAlloc�����л��ٴ�ת��Ϊ�����ֽ�����
				if (_memory == nullptr)
				{
					throw std::bad_alloc();//����ʧ�ܾ����쳣
				}
			}

			obj = (T*)_memory;
			size_t objsize = sizeof(T) < sizeof(void*) ? sizeof(void*) : sizeof(T);//����T������Ҫ���ڴ��С�ж����ÿ�η�����ڴ�Ӧ�ô��ڵ��ڵ�ǰ������һ��ָ��Ĵ�С���Ӷ���֤����˳�������һ�����ĵ�ַ
			_memory += objsize;
			_remainBytes -= objsize;
		}


		//��λnew����ʾ����T�Ĺ��캯����ʼ��
		new(obj)T;
		return obj;
	}

	//�����ڴ�
	void Delete(T* obj)//����ָ��Ҫ���յĶ����ָ��
	{
		//��ʾ�������������������
		obj->~T();

		/*���Բ����������Ƿ�Ϊ�յ������ֱ��ͷ�弴�ɣ���Ϊ_freelist��ʼΪ�գ��������д�����ԣ�
		if(_freelist == nullptr)//����Ϊ�վ���ͷ��
		{
			_freelist = obj;
			//*(int*)obj = nullptr;//��̭
			*(void**)obj = nullptr;
		}
		else//ͷ��
		{
			*(void**)obj = _freelist;
			_freelist = obj;
		}
		*/

		//�޸ĺ�
		* (void**)obj = _freelist;
		_freelist = obj;
	}
	std::mutex _poolMtx;//����ΪObjectPool����
private:

	char* _memory = nullptr;//ָ�����ڴ��ָ��
	size_t _remainBytes = 0;//����ڴ����зֹ�����ʣ���ֽ���
	void* _freelist = nullptr;//����������Ϊ�����ڴ�Ķ���������ǲ�ȷ��������Ҫʹ��void*
};