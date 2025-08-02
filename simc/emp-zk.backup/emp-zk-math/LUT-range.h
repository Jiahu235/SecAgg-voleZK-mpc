#pragma once // 这个命令好使的！！爱了爱了

#include "emp-zk/emp-zk-arith/zk_fp_exec.h"
#include "emp-zk/emp-zk-arith/zk_fp_exec_prover.h"
#include "emp-zk/emp-zk-arith/zk_fp_exec_verifier.h"
#include "emp-zk/emp-zk-arith/triple_auth.h"
#include "emp-zk/emp-zk-arith/ostriple.h"
#include "emp-zk/emp-zk-arith/int_fp.h"
#include "emp-zk/emp-zk-arith/conversion.h"
#include "emp-zk/emp-zk-arith/polynomial.h"

#include "emp-zk/emp-vole/utility.h" // 里面有PR

// template<typename IO>
class LUTRangeIntFp
{
public:
	//
	int party;

	// 以下三个是双方都能明文知道的
	vector<uint64_t> writes_index;
	vector<uint64_t> writes_version;

	// 以下一个是ALICE维护的，只有ALICE知道
	vector<uint64_t> latest_version;

	//
	vector<IntFp> writesMAC_index;
	vector<IntFp> writesMAC_version;
	//
	vector<IntFp> readsMAC_index;
	vector<IntFp> readsMAC_version;
	// 存放check中补全table时，两方都知道的index
	vector<uint64_t> readsnoMAC_index;
	//
	uint64_t read_step = 0;

	//
	uint64_t simplecheck = 0;
	vector<IntFp> batch_simplecheck;

	LUTRangeIntFp(int _party) : party(_party) {}

	~LUTRangeIntFp() {
		if(read_step != 0) 
			LUTRangecheck();
	}

	// LUTRangeinit(): initiate the list writes, including version, and index; initiate a vector latest_version
	void LUTRangeinit(uint64_t lutSize)
	{
		// lutSize是明文LUT的item数量
		for (int i = 0; i < lutSize; i++)
		{
			writes_version.push_back(0);
			writes_index.push_back(i);

			if (party == ALICE)
				latest_version.push_back(0);
		}
	}

	// LUTRangeread(): input (IntFp) index
	void LUTRangeread(IntFp &index)
	{
		// uint64_t clear_index = index.reveal(); // 这里不能这样写，因为这样的话clear_index就暴露给两方了
		uint64_t clear_index = 0;
		uint64_t version = 0;

		if (party == ALICE){
			clear_index = (uint64_t)HIGH64(index.value); // clear_index应该这样写
			version = latest_version[clear_index];
		}

		IntFp version_read = IntFp(version, ALICE);

		readsMAC_index.push_back(index);
		readsMAC_version.push_back(version_read);

		writesMAC_index.push_back(index);
		writesMAC_version.push_back(version_read + 1);

		if (party == ALICE){
			latest_version[clear_index] = version + 1;
		}

		++read_step;

		if (read_step == writes_index.size() * 8) {// TODO: 该check batchsize需要确定？？
			LUTRangecheck();					 
			read_step = 0;
		}
	}

	// LUTRange_Mac_vector_inn_prdt(): each element in MacedList should + r, and then perform element-wise multiplication
	IntFp LUTRange_Mac_vector_inn_prdt(vector<IntFp> &MacedList, uint64_t r)
	{
		// TODO: 此处转化为低阶多项式，用QuickSilver的多项式方法优化，具体可参考ram的做法 ？？
		IntFp out = MacedList[0] + r;
		for (int i = 1; i < MacedList.size(); i++){
			out = out * (MacedList[i] + r);
		}

		return out;
	}

	// LUTRangecheck_permutation(): check whether reads is a permutation of writes
	void LUTRangecheck_permutation(vector<IntFp> &readsMac, vector<uint64_t> &writesNoMac, vector<IntFp> &writesMac, uint64_t r)
	{
		// 将readsMac和writesMac转换为多项式并计算输入为r时的结果
		IntFp readsOut = LUTRange_Mac_vector_inn_prdt(readsMac, r);
		IntFp writesOut = LUTRange_Mac_vector_inn_prdt(writesMac, r);

		// writesOut中添加没有Mac的部分
		// uint64_t PR = 2305843009213693951;
		uint64_t writestmp = add_mod(writesNoMac[0], r);
		for (int i = 1; i < writesNoMac.size(); i++){
			writestmp = mult_mod(writestmp, add_mod(writesNoMac[i], r));
		}

		writesOut = writesOut * writestmp;

		// checkZero
		IntFp checkzero = readsOut + writesOut.negate();

		// cout << "writesOut = " << writesOut.reveal() << endl;
		// cout << "readsOut = " << readsOut.reveal() << endl;
		// cout << "checkzero = " << checkzero.reveal() << endl;

		checkzero.reveal_zero(); // TODO: 是否应该调用该函数 需确认？？ 要调用其他函数 还是用ram F2k中的方式？？
	}

	// LUTRangecheck() needs parameters：readsMAC_XXX, writesMAC_XXX, writes_XXX; batch check the correctness of read-operation
	void LUTRangecheck()
	{
		uint64_t tmp_version;

		// reads中没有的n个元素要补全
		for (int i = 0; i < writes_index.size(); i++)
		{
			// 以下一个是PUBLIC，因为index就是函数f对应的LUT值，两方都知道;既然两方都知道，放过去的时候也没必要做MAC
			// readsMAC_index.push_back(IntFp((uint64_t)i, PUBLIC));
			readsnoMAC_index.push_back(i);

			// 以下一个是ALICE，因为latest_version只有alice知道
			// readsMAC_version.push_back(IntFp(latest_version[i], ALICE)); // 这样写会报错！！
			if (party == ALICE){
				tmp_version = latest_version[i];
			}
			readsMAC_version.push_back(IntFp(tmp_version, ALICE));
		}

		// check writes和reads的size是否相同
		assert((readsMAC_index.size() + readsnoMAC_index.size()) == (writes_index.size() + writesMAC_index.size()));

		// generate random a[] for packing
		// uint64_t PR = 2305843009213693951;
		uint64_t *a = new uint64_t[3]; // 简化 不用发送

		__uint128_t *randomness = new __uint128_t[3];
		PRG prg(fix_key);
		prg.random_block((block *)randomness, 3);
		for (int i = 0; i < 3; i++){
			a[i] = randomness[i] % PR;
		}

		// Packing writes
		vector<uint64_t> writesNoMacPackList;
		vector<IntFp> writesPackList;
		uint64_t tNoMacPack = 0;
		IntFp tPack; //
		for (int i = 0; i < writes_index.size(); i++)
		{
			tNoMacPack = add_mod(mult_mod(writes_index[i], a[0]), mult_mod(writes_version[i], a[1]));
			writesNoMacPackList.push_back(tNoMacPack);
		}
		for (int i = 0; i < writesMAC_index.size(); i++)
		{
			tPack = writesMAC_index[i] * a[0] + writesMAC_version[i] * a[1];
			writesPackList.push_back(tPack);
		}

		// Packing reads
		vector<IntFp> readsPackList;
		for (int i = 0; i < readsMAC_index.size(); i++)
		{
			tPack = readsMAC_index[i] * a[0] + readsMAC_version[i] * a[1];
			readsPackList.push_back(tPack);
		}
		for (int i = 0; i < readsnoMAC_index.size(); i++)
		{
			tPack = readsMAC_version[readsMAC_index.size() + i] * a[1] + mult_mod(readsnoMAC_index[i], a[0]);
			readsPackList.push_back(tPack);
		}

		// check packing后，writes和reads的size是否相同
		assert(readsPackList.size() == (writesNoMacPackList.size() + writesPackList.size()));

		// check reads and writes 是否互为permutation
		LUTRangecheck_permutation(readsPackList, writesNoMacPackList, writesPackList, a[2]);

		// resize reads and writesMac
		readsMAC_index.resize(0);
		readsMAC_version.resize(0);
		writesMAC_index.resize(0);
		writesMAC_version.resize(0);
		readsnoMAC_index.resize(0);

		// 将latest_version中的所有元素置为0
		if (party == ALICE){
			std::fill(latest_version.begin(), latest_version.end(), 0);
		}

		read_step = 0;
	}

	// LUTRangecheck(): simple check - batch check zero
	void LUTRangeSimplecheck(IntFp &index)
	{
		uint64_t end = writes_index.size();
		IntFp tmp = index;
		for (int i = 1; i < end; i++){
			tmp = tmp * (index + (PR - i));
		}

		batch_simplecheck.push_back(tmp);
		++simplecheck;

		// TODO: 这里是否不需要手动判断是否要batchcheckzero，而是直接传入input，batch_reveal_check_zero内部自然会一个batch一个batch的做check呢？？？
		if (simplecheck == writes_index.size() * 8)
		{ // TODO：confirm batchcheck size
			bool res = batch_reveal_check_zero(batch_simplecheck.data(), batch_simplecheck.size());

			if (!res)
				error("batch_reveal_check_zero failed");

			simplecheck = 0;
			batch_simplecheck.resize(0);
		}
	}
};