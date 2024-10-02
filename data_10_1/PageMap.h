#pragma once
#include "Common.h"

//�����������һ����ֱ�Ӷ�ַ����ӳ��Ĺ�ϣ�����е� K-V ��ϵ���� <ҳ�ţ�span*>
template <int BITS>
class TCMalloc_PageMap1 {
private:
	static const int LENGTH = 1 << BITS; // ��ϣ�� / ����ĳ��ȣ���1^BITS
	void** array_; // ������ÿ��λ�ô�ŵĶ���ָ��span�Ķ���ָ�루������SpanList�Ķ���ָ�룩

public:
	typedef uintptr_t Number;//����ҳ�ŵ�ȡֵ����Ϊuintptr_t��32λ�Ļ����ϱ�ʾunsigned long int��64λ�Ļ����ϱ�ʾunsigned int

	//��������ռ�
	explicit TCMalloc_PageMap1()
	{
		size_t size = sizeof(void*) << BITS;//Ҫ���ٵĿռ��С
		size_t alignSize = SizeClass::_RoundUp(size, 1 << PAGE_SHIFT);//�ڴ����
		array_ = (void**)SystemAlloc(alignSize >> PAGE_SHIFT);//������룬ʹ��ObjectPool�е�New()Ҳ��
		memset(array_, 0, sizeof(void*) << BITS);//�����뵽�Ŀռ�ȫ����Ϊ0
	}

	//ͨ��ҳ������ȡ�����е�span
	void* get(Number k) const 
	{ // ͨ��k����ȡ��Ӧ��ָ��
		if ((k >> BITS) > 0) 
		{
			return NULL;
		}
		return array_[k];
	}

	
	//��span���������У�������ҳ�� k��span v��ӳ���ϵ
	void set(Number k, void* v) 
	{ // ��v���õ�k�±�
		array_[k] = v;
	}
};


//˫�������
template <int BITS>
class TCMalloc_PageMap2 {
private:
	//32λ������
	static const int ROOT_BITS = 5; //ǰ5λ��Ϊ��һ�������������
	static const int ROOT_LENGTH = 1 << ROOT_BITS;//�����СΪ2^5

	static const int LEAF_BITS = BITS - ROOT_BITS; //ʣ��λ����14λ��Ϊ��һ�������������
	static const int LEAF_LENGTH = 1 << LEAF_BITS; //�����СΪ2^14

	//����������LEAF_LENGTH��С��ָ������
	struct Leaf 
	{ 
		void* values[LEAF_LENGTH];//�����е�ÿ��λ�ô�ŵĶ���span*��ӳ����ģ�ûӳ��Ļ��ǿգ�
	};

	Leaf* root_[ROOT_LENGTH]; //һ������������д�ŵĶ���һ����Leaf*���͵�����
public:
	typedef uintptr_t Number;

	//���ٿռ�
	explicit TCMalloc_PageMap2() 
	{ 
		memset(root_, 0, sizeof(root_));//������ȫ���ÿ�
		PreallocateMoreMemory();//��Ϊֻ��Ҫ2MB��С�Ŀռ䣬��������ֱ�ӽ����еĿռ俪����ϣ������ǽ�ʡ�ڴ�ռ��������
	}

	// ��ǰ���ÿռ䣬����Ͱ�2M��ֱ�ӿ���
	void PreallocateMoreMemory()
	{
		Ensure(0, 1 << BITS);
	}

	// ȷ�����е�һ������Ӧ�Ķ������������ռ��Ѿ��������
	bool Ensure(Number start, size_t n)
	{
		for (Number key = start; key <= start + n - 1;)//key��ʾһ�����������ÿ��λ�ÿ��ٶ�����������������ҳ��
		{
			const Number i1 = key >> LEAF_BITS;//il��ʾҳ�ŵĶ����Ʊ�ʾ�е�ǰ5��������������һ�����������Ĵ�С��ʮ���ƣ�����Ϊ����ҳ��������145λ

			//il��С���ܴ���32
			if (i1 >= ROOT_LENGTH)
			{
				return false;
			}

			// ���û���þͿ��ռ�
			if (root_[i1] == NULL)
			{
				static ObjectPool<Leaf>	leafPool;//Leaf���͵Ķ����СΪ2^14�ֽڣ���ObjectPool�п��ٵĴ�СΪ128KB��2^17�ֽڣ���ȫ����
				Leaf* leaf = (Leaf*)leafPool.New();

				memset(leaf, 0, sizeof(*leaf));//��������Ķ������������ȫ���ÿ�
				root_[i1] = leaf;//һ������������±�Ϊil��λ�ô�ŵ�ָ��������leaf
			}

			//һ������������СΪ32�����ֱ����key��Ϊ��ͨ��ҳ�Ŵ�0�ӵ�2^19��ѭ������̫����
			// ��������ֱ����ǰkey��ǰ5���ֽ�+1����ֱ��������һ�������±괦�����ж�
			// ����key = 8192ʱ�������Ʊ�ʾΪ0000 0000 0000 0 [000 0010 0000 0000 0000]�������ѭ��8192�ζ������ж�һ��������������±�Ϊ0��λ���Ƿ񿪱ٳɹ��ռ䣬ʮ���˷���Դ
			// ������������key = 0ʱ��ֱ������14λ��+1Ȼ�����ƶ��������õ��Ķ����Ʊ�ʾ����0000 0000 0000 0 [000 0100 0000 0000 0000]
			// ������һ��ѭ��ʱ�жϵľ���һ��������������±�Ϊ0��λ���Ƿ񿪱ٳɹ��ռ䣬��������Ҫ32��ѭ���������
			key = ((key >> LEAF_BITS) + 1) << LEAF_BITS;
		}
		return true;
	}

	//��ȡҳ�Ŷ�Ӧ��span
	void* get(Number k) const 
	{
		const Number i1 = k >> LEAF_BITS;
		const Number i2 = k & (LEAF_LENGTH - 1);
		if ((k >> BITS) > 0 || root_[i1] == NULL) 
		{
			return NULL;
		}
		return root_[i1]->values[i2];
	}

	//����ҳ�ź�span��ӳ��
	void set(Number k, void* v) 
	{
		const Number i1 = k >> LEAF_BITS;
		const Number i2 = k & (LEAF_LENGTH - 1);
		ASSERT(i1 < ROOT_LENGTH);
		root_[i1]->values[i2] = v;
	}
};
