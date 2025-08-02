#include "emp-tool/emp-tool.h"
#include "emp-zk/emp-zk.h"
#include <iostream>

#define ZK_F 16
#define ZK_INT_LEN 62

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

void test_sigmoid(BoolIO<NetIO> *ios[threads], int party, IntFp *x, IntFp *y, int size)
{
	Integer zero(ZK_INT_LEN, 0, PUBLIC);
  	Integer one(ZK_INT_LEN, 1, PUBLIC);
	Integer smallest_neg(ZK_INT_LEN, (uint64_t)((PR - 1) / 2) + 1, PUBLIC); 

	// Step 1: compute b
	vector<Integer> x_integer(size);
	IntFp *b_fp = new IntFp[size];
	arith2bool<BoolIO<NetIO>>(x_integer.data(), x, size); 
	vector<Integer> b_integer(size);
  	for (size_t i = 0; i < size; ++i){
		b_integer[i] = one.select(x_integer[i].geq(smallest_neg), zero);
	}
	bool2arith<BoolIO<NetIO>>(b_fp, b_integer.data(), size); 

	// step 2: compute x_bar
	IntFp *x_bar_fp = new IntFp[size];
	for (int i = 0; i < size; i++){
		x_bar_fp[i] = (b_fp[i] * 2 + (PR - 1)) * x[i];
	}

	// step 3: compute exp
}

int main(int argc, char **argv)
{
	parse_party_and_port(argv, &party, &port);
	BoolIO<NetIO> *ios[threads];
	for (int i = 0; i < threads; ++i)
		ios[i] = new BoolIO<NetIO>(new NetIO(party == ALICE ? nullptr : "127.0.0.1", port + i), party == ALICE);

	std::cout << std::endl
			  << "------------ Div zero-knowledge proof test ------------" << std::endl
			  << std::endl;

	setup_zk_bool<BoolIO<NetIO>>(ios, threads, party);
	setup_zk_arith<BoolIO<NetIO>>(ios, threads, party, true);

	sync_zk_bool<BoolIO<NetIO>>();

	cout << "start data generation" << endl;

	srand(time(NULL));

	uint64_t *a = new uint64_t[size];
	for (int i = 0; i < size; ++i)
	{
		// cout << i << endl;
		a[i] = rand() % PR;
	}

	IntFp *a_zks = new IntFp[size];
	IntFp *output = new IntFp[size];
	batch_feed(a_zks, a, size);

	cout << "start test" << endl;

	uint64_t com1 = comm(ios);
	auto start = clock_start();

	test_sigmoid(ios, party, a_zks, output, size);

	finalize_zk_bool<BoolIO<NetIO>>();
	finalize_zk_arith<BoolIO<NetIO>>();
	
	double time1 = time_from(start);
	cout << "time - div: " << time1 / 1000 << " ms\t " << party << endl;
	uint64_t com11 = comm(ios) - com1;
	std::cout << "communication - div (KB): " << com11 / 1024.0 << std::endl;

	cout << "finish test" << endl;

	for (int i = 0; i < threads; ++i)
	{
		delete ios[i]->io;
		delete ios[i];
	}
	return 0;
}
