#pragma once
#include "Common.h"

//单层基数树：一个用直接定址法来映射的哈希表，其中的 K-V 关系就是 <页号，span*>
template <int BITS>
class TCMalloc_PageMap1 {
private:
	static const int LENGTH = 1 << BITS; // 哈希表 / 数组的长度，即1^BITS
	void** array_; // 数组中每个位置存放的都是指向span的二级指针（类似于SpanList的二级指针）

public:
	typedef uintptr_t Number;//定义页号的取值类型为uintptr_t，32位的机器上表示unsigned long int，64位的机器上表示unsigned int

	//开辟数组空间
	explicit TCMalloc_PageMap1()
	{
		size_t size = sizeof(void*) << BITS;//要开辟的空间大小
		size_t alignSize = SizeClass::_RoundUp(size, 1 << PAGE_SHIFT);//内存对齐
		array_ = (void**)SystemAlloc(alignSize >> PAGE_SHIFT);//向堆申请，使用ObjectPool中的New()也行
		memset(array_, 0, sizeof(void*) << BITS);//将申请到的空间全部置为0
	}

	//通过页号来获取数组中的span
	void* get(Number k) const 
	{ // 通过k来获取对应的指针
		if ((k >> BITS) > 0) 
		{
			return NULL;
		}
		return array_[k];
	}

	
	//将span放入数组中，即建立页号 k和span v的映射关系
	void set(Number k, void* v) 
	{ // 将v设置到k下标
		array_[k] = v;
	}
};


//双层基数树
template <int BITS>
class TCMalloc_PageMap2 {
private:
	//32位机器下
	static const int ROOT_BITS = 5; //前5位作为第一层基数树的数组
	static const int ROOT_LENGTH = 1 << ROOT_BITS;//数组大小为2^5

	static const int LEAF_BITS = BITS - ROOT_BITS; //剩余位，即14位作为第一层基数树的数组
	static const int LEAF_LENGTH = 1 << LEAF_BITS; //数组大小为2^14

	//二层数组是LEAF_LENGTH大小的指针数组
	struct Leaf 
	{ 
		void* values[LEAF_LENGTH];//数组中的每个位置存放的都是span*（映射过的，没映射的还是空）
	};

	Leaf* root_[ROOT_LENGTH]; //一层基数树数组中存放的都是一个个Leaf*类型的数组
public:
	typedef uintptr_t Number;

	//开辟空间
	explicit TCMalloc_PageMap2() 
	{ 
		memset(root_, 0, sizeof(root_));//将数组全部置空
		PreallocateMoreMemory();//因为只需要2MB大小的空间，所以这里直接将所有的空间开辟完毕，不考虑节省内存空间的问题了
	}

	// 提前开好空间，这里就把2M的直接开好
	void PreallocateMoreMemory()
	{
		Ensure(0, 1 << BITS);
	}

	// 确保所有的一层和其对应的二层基数树数组空间已经开辟完毕
	bool Ensure(Number start, size_t n)
	{
		for (Number key = start; key <= start + n - 1;)//key表示一层基数树数组每个位置开辟二层基数树数组所需的页号
		{
			const Number i1 = key >> LEAF_BITS;//il表示页号的二进制表示中的前5个二进制数，即一层基数树数组的大小（十进制），因为它将页号左移了145位

			//il大小不能大于32
			if (i1 >= ROOT_LENGTH)
			{
				return false;
			}

			// 如果没开好就开空间
			if (root_[i1] == NULL)
			{
				static ObjectPool<Leaf>	leafPool;//Leaf类型的对象大小为2^14字节，而ObjectPool中开辟的大小为128KB即2^17字节，完全够用
				Leaf* leaf = (Leaf*)leafPool.New();

				memset(leaf, 0, sizeof(*leaf));//将新申请的二层基数树数组全部置空
				root_[i1] = leaf;//一层基数树数组下标为il的位置存放的指针数组是leaf
			}

			//一层基数树数组大小为32，如果直接让key作为普通的页号从0加到2^19，循环次数太多了
			// 现在我们直接让前key的前5个字节+1，即直接跳到下一个数组下标处进行判断
			// 比如key = 8192时，二进制表示为0000 0000 0000 0 [000 0010 0000 0000 0000]，这表明循环8192次都是在判断一层基数树数组中下标为0的位置是否开辟成功空间，十分浪费资源
			// 但现在我们在key = 0时，直接右移14位再+1然后再移动回来，得到的二进制表示就是0000 0000 0000 0 [000 0100 0000 0000 0000]
			// 这样下一次循环时判断的就是一层基数树数组中下标为0的位置是否开辟成功空间，这样仅需要32次循环即可完成
			key = ((key >> LEAF_BITS) + 1) << LEAF_BITS;
		}
		return true;
	}

	//获取页号对应的span
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

	//建立页号和span的映射
	void set(Number k, void* v) 
	{
		const Number i1 = k >> LEAF_BITS;
		const Number i2 = k & (LEAF_LENGTH - 1);
		ASSERT(i1 < ROOT_LENGTH);
		root_[i1]->values[i2] = v;
	}
};
