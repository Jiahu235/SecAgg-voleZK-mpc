#include "emp-tool/emp-tool.h"
#include "emp-zk/emp-zk.h"
#include <iostream>

using namespace emp;
using namespace std;

int port, party;
const int threads = 1;

int LUT_size = 5;



uint64_t comm(BoolIO<NetIO> *ios[threads])
{
	uint64_t c = 0;
	for (int i = 0; i < threads; ++i)
		c += ios[i]->counter;
	return c;
}

void test_PolyInterpolate(BoolIO<NetIO> *ios[threads], int party)
{
	uint64_t com1 = comm(ios);
	auto start = clock_start();

	Poly *poly = new Poly();
    // Prior_Poly *poly = new Prior_Poly();

	std::vector<ZpMersenneIntElement> coeff(LUT_size), X(LUT_size), Y(LUT_size);

	for (int i = 0; i < LUT_size; i++){
		X.at(i).elem = i;
		Y.at(i).elem = 3 * i * i + 4;
	}

	poly->interpolateMersenne(coeff, X, Y);

	ZpMersenneIntElement a, b;
	a.elem = 3;

	poly->evalMersenne(b, coeff, a);

	cout << "plaintext result = " << b.elem << endl;
	cout << "-------------------------------------------------" << b.elem << endl;

	IntFp x = IntFp((uint64_t)3, PUBLIC);

	IntFp y;

    poly->IntFp_evalMersenne(y, coeff, x);

    cout << "IntFp result = " << y.reveal() << endl;

	double time2 = time_from(start);

	std::cout << "time: " << time2 / 1000 << " ms\t " << party << endl;

	uint64_t com2 = comm(ios) - com1;
	std::cout << "communication (KB): " << com2 / 1024.0 << std::endl;
}


int main(int argc, char **argv)
{
	parse_party_and_port(argv, &party, &port);
	BoolIO<NetIO> *ios[threads];
	for (int i = 0; i < threads; ++i)
		ios[i] = new BoolIO<NetIO>(new NetIO(party == ALICE ? nullptr : "127.0.0.1", port + i), party == ALICE);

	std::cout << std::endl
			  << "------------ Exp_exact zero-knowledge proof test ------------" << std::endl
			  << std::endl;

	setup_zk_bool<BoolIO<NetIO>>(ios, threads, party);
	setup_zk_arith<BoolIO<NetIO>>(ios, threads, party, true);

	sync_zk_bool<BoolIO<NetIO>>();

	test_PolyInterpolate(ios, party);

	cout << "finish test" << endl;

	finalize_zk_bool<BoolIO<NetIO>>();
	finalize_zk_arith<BoolIO<NetIO>>();

	for (int i = 0; i < threads; ++i)
	{
		delete ios[i]->io;
		delete ios[i];
	}
	return 0;
}
