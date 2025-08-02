#include "emp-tool/emp-tool.h"
#include "emp-zk/emp-zk.h"
#include <iostream>

#define ZK_F 16

using namespace emp;
using namespace std;

int port, party;
const int threads = 1;

int size = 10000;

uint64_t comm(BoolIO<NetIO> *ios[threads])
{
	uint64_t c = 0;
	for (int i = 0; i < threads; ++i)
		c += ios[i]->counter;
	return c;
}

void test_rsqrt(BoolIO<NetIO> *ios[threads], int party, IntFp *a_zks, IntFp *output, int size)
{
	// Step 1:FP to Integer
	vector<Integer> a_Int(size);
	// vector<IntFp> res_Int(size);
	// sync_zk_bool<BoolIO<NetIO>>();

	arith2bool<BoolIO<NetIO>>(a_Int.data(), a_zks, size); // 该function内部有check阶段

	// double time1 = time_from(start);
	// cout << "time - A2B: " << time1 / 1000 << " ms\t " << party << endl;
	// uint64_t com11 = comm(ios) - com1;
	// std::cout << "communication - A2B (KB): " << com11 / 1024.0 << std::endl;

	// Step 2:calc exp
	Float ir_res(0., PUBLIC);
	Float one(1.0, PUBLIC);

	for (auto i = 0; i < size; i++)
	{
		ir_res = Int62ToFloat(a_Int[i], ZK_F); // 该过程所有的操作符都已被rewrite，最终都会表示成circuit的形式，check在AND gate协议中给出
		ir_res = ir_res.sqrt();				   // 有一个全局变量统计AND门次数，一旦大于batchcheck size, 在调用AND gate协议时，将先check
		ir_res = (one / ir_res);
		a_Int[i] = FloatToInt62(ir_res, ZK_F); // 同样表示成circuit的形式，check在AND gate协议中给出
	}

	// double time2 = time_from(start) - time1;
	// cout << "time - Circuit eveluation: " << time2 / 1000 << " ms\t " << party << endl;
	// uint64_t com2 = comm(ios) - com11;
	// std::cout << "communication - Circuit eveluation (KB): " << com2 / 1024.0 << std::endl;

	// Step 3: Bool to Arithmetic
	bool2arith<BoolIO<NetIO>>(output, a_Int.data(), size); // 该function内部有check阶段

	// double time3 = time_from(start) - time1 - time2;
	// cout << "time - B2A: " << time3 / 1000 << " ms\t " << party << endl;
	// uint64_t com3 = comm(ios) - com2;
	// std::cout << "communication - B2A (KB): " << com3 / 1024.0 << std::endl;
}

int main(int argc, char **argv)
{
	parse_party_and_port(argv, &party, &port);
	BoolIO<NetIO> *ios[threads];
	for (int i = 0; i < threads; ++i)
		ios[i] = new BoolIO<NetIO>(new NetIO(party == ALICE ? nullptr : "127.0.0.1", port + i), party == ALICE);

	std::cout << std::endl
			  << "------------ rsqrt zero-knowledge proof test ------------" << std::endl
			  << std::endl;

	setup_zk_bool<BoolIO<NetIO>>(ios, threads, party);
	setup_zk_arith<BoolIO<NetIO>>(ios, threads, party, true);

	sync_zk_bool<BoolIO<NetIO>>();

	cout << "start data generation" << endl;

	srand(time(NULL));

	uint64_t *a = new uint64_t[size];
	for (int i = 0; i < size; i++){
		if (party == ALICE){
			while (a[i] == 0)
			{
				a[i] = rand();
			}
			a[i] = a[i] % PR;
		}
	}

	IntFp *a_zks = new IntFp[size];
	IntFp *output = new IntFp[size];
	batch_feed(a_zks, a, size);

	cout << "start test" << endl;

	uint64_t com1 = comm(ios);
	auto start = clock_start();

	test_rsqrt(ios, party, a_zks, output, size);

	finalize_zk_bool<BoolIO<NetIO>>();
	finalize_zk_arith<BoolIO<NetIO>>();

	double time1 = time_from(start);
	cout << "time - rsqrt: " << time1 / 1000 << " ms\t " << party << endl;
	uint64_t com11 = comm(ios) - com1;
	std::cout << "communication - rsqrt (KB): " << com11 / 1024.0 << std::endl;

	cout << "finish test" << endl;

	for (int i = 0; i < threads; ++i)
	{
		delete ios[i]->io;
		delete ios[i];
	}
	return 0;
}
