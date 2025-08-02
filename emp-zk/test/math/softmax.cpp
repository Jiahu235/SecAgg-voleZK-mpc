#include "emp-tool/emp-tool.h"
#include "emp-zk/emp-zk.h"
#include <iostream>

using namespace emp;
using namespace std;

int port, party;
const int threads = 1;

int rows = 250;
int cols = 400;

uint64_t comm(BoolIO<NetIO> *ios[threads])
{
	uint64_t c = 0;
	for (int i = 0; i < threads; ++i)
		c += ios[i]->counter;
	return c;
}

int main(int argc, char **argv)
{
	parse_party_and_port(argv, &party, &port);
	BoolIO<NetIO> *ios[threads];
	for (int i = 0; i < threads; ++i)
		ios[i] = new BoolIO<NetIO>(new NetIO(party == ALICE ? nullptr : "127.0.0.1", port + i), party == ALICE);

	std::cout << std::endl
			  << "------------ ZKSoftmax test ------------" << std::endl
			  << std::endl;

	setup_zk_bool<BoolIO<NetIO>>(ios, threads, party);
	setup_zk_arith<BoolIO<NetIO>>(ios, threads, party, true);

	sync_zk_bool<BoolIO<NetIO>>();

	uint64_t *witness = new uint64_t[rows * cols];
	memset(witness, 0, rows * cols * sizeof(uint64_t));

	startComputation(party);

    IntFp *x = new IntFp[rows * cols];
	IntFp *y = new IntFp[rows * cols];
	for (int i = 0; i < rows * cols; i++){
		if (party == ALICE){
			witness[i] = rand();
			witness[i] = witness[i] & ((1ULL << DIV_N) - 1);  
			witness[i] = witness[i] % PR;
		}
		x[i] = IntFp(witness[i], ALICE);
	}
	ZKSoftmax(party, x, y, rows, cols);

	endComputation(party);

	cout << "finish test" << endl;

	finalize_zk_bool<BoolIO<NetIO>>();
	finalize_zk_arith<BoolIO<NetIO>>();

	for (int i = 0; i < threads; i++)
	{
		delete ios[i]->io;
		delete ios[i];
	}
	return 0;
}
