#include "emp-tool/emp-tool.h"
#include "emp-zk/emp-zk.h"
#include <iostream>

using namespace emp;
using namespace std;

int port, party;
const int threads = 1;

int dim = 10000;

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
			  << "------------ ZKpositiveTruncAny test ------------" << std::endl
			  << std::endl;

	setup_zk_bool<BoolIO<NetIO>>(ios, threads, party);
	setup_zk_arith<BoolIO<NetIO>>(ios, threads, party, true);

	sync_zk_bool<BoolIO<NetIO>>();

	uint64_t *witness = new uint64_t[dim];
	memset(witness, 0, dim * sizeof(uint64_t));

	startComputation(party);

	IntFp *x = new IntFp[dim];
	IntFp *y = new IntFp[dim];
	for (int i = 0; i < dim; i++){
		if (party == ALICE){
			witness[i] = rand() %  ((BIT_LENGTH - 1) / 2 + 1);
			witness[i] = witness[i] % PR;
		}
		x[i] = IntFp(witness[i], ALICE);
	}
	uint64_t trunclen = SCALE;
	ZKpositiveTruncAny(party, x, y, dim, trunclen);
	/****************************/
	/**** verify correctness ****/
	/****************************/
	if (party == ALICE){
		for (int i = 0; i < dim; i++){
			uint64_t digit_mask = (1ULL << (BIT_LENGTH - trunclen)) - 1;
			uint64_t ori_y = (witness[i] >> trunclen) & digit_mask;
			uint64_t nm_y = (uint64_t)HIGH64(y[i].value);
			if (ori_y != nm_y){
				cout << "fault !!!" << endl;
			}
		}
	}

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
