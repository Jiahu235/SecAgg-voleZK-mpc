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
			  << "------------ ZKCompare test ------------" << std::endl
			  << std::endl;

	setup_zk_bool<BoolIO<NetIO>>(ios, threads, party);
	setup_zk_arith<BoolIO<NetIO>>(ios, threads, party, true);

	sync_zk_bool<BoolIO<NetIO>>();

	uint64_t *witness = new uint64_t[dim];
	memset(witness, 0, dim * sizeof(uint64_t));

	startComputation(party);

	cout << "ZKCompareShift test" << endl;
	uint64_t shiftlen = ceil(log2(PR)) - 1;
    IntFp *xShift = new IntFp[dim];
	IntFp *yShift = new IntFp[dim];
	for (int i = 0; i < dim; i++)
	{
		if (party == ALICE){
			witness[i] = rand() % PR;
		} 
		xShift[i] = IntFp(witness[i], ALICE);
	}
	ZKCompareShift(xShift, yShift, shiftlen, dim);
	/****************************/
	/**** verify correctness ****/
	/****************************/
	if (party == ALICE){
		for (int i = 0; i < dim; i++){
			uint64_t ori_y = 0;
			if ( (witness[i] > (1ULL << shiftlen)) || (witness[i] == (1ULL << shiftlen)) ){
				ori_y = 1;
			}
			uint64_t nm_y = (uint64_t)HIGH64(yShift[i].value);
			if (ori_y != nm_y){
				cout << "fault !!!" << endl;
			}
		}
	}

	cout << "ZKCompareConstant test" << endl;
	uint64_t y = (PR - 1)/2;
    IntFp *xConstant = new IntFp[dim];
	IntFp *zConstant = new IntFp[dim];
	for (int i = 0; i < dim; i++)
	{
		if (party == ALICE){
			witness[i] = rand() % PR;
		} 
		xConstant[i] = IntFp(witness[i], ALICE);
	}
	ZKCompareConstant(xConstant, y, zConstant, dim);
	/****************************/
	/**** verify correctness ****/
	/****************************/
	if (party == ALICE){
		for (int i = 0; i < dim; i++){
			uint64_t ori_z = 0;
			if ( (witness[i] < y) || (witness[i] == y) ){
				ori_z = 1;
			}
			uint64_t nm_z = (uint64_t)HIGH64(zConstant[i].value);
			if (ori_z != nm_z){
				cout << "fault !!!" << endl;
			}
		}
	}

	cout << "ZKFpCompare test" << endl;
	uint64_t *witness1 = new uint64_t[dim];
    IntFp *xFp = new IntFp[dim];
	IntFp *yFp = new IntFp[dim];
	IntFp *zFp = new IntFp[dim];
	for (int i = 0; i < dim; i++)
	{
		if (party == ALICE){
			witness[i] = rand() % PR;
			witness1[i] = rand() % PR;
		} 
		xFp[i] = IntFp(witness[i], ALICE);
		yFp[i] = IntFp(witness1[i], ALICE);
	}
	ZKFpCompare(xFp, yFp, zFp, dim);
	/****************************/
	/**** verify correctness ****/
	/****************************/
	if (party == ALICE){
		for (int i = 0; i < dim; i++){
			uint64_t ori_z = 0;
			if ( witness[i] < witness1[i] ){
				ori_z = 1;
			}
			uint64_t nm_z = (uint64_t)HIGH64(zFp[i].value);
			if (ori_z != nm_z){
				cout << "fault !!!" << endl;
			}
		}
	}

	cout << "ZKFpCompareLEQ test" << endl;
	// uint64_t *witness1 = new uint64_t[dim];
    IntFp *xLEQ = new IntFp[dim];
	IntFp *yLEQ = new IntFp[dim];
	IntFp *zLEQ = new IntFp[dim];
	for (int i = 0; i < dim; i++)
	{
		if (party == ALICE){
			witness[i] = rand() % PR;
			witness1[i] = rand() % PR;
			while (witness1[i] == (PR - 1))   //保证+1后不溢出
			{
				witness[i] = rand();
			}
		} 
		xLEQ[i] = IntFp(witness[i], ALICE);
		yLEQ[i] = IntFp(witness1[i], ALICE);
	}
	ZKFpCompareLEQ(xLEQ, yLEQ, zLEQ, dim);
	/****************************/
	/**** verify correctness ****/
	/****************************/
	if (party == ALICE){
		for (int i = 0; i < dim; i++){
			uint64_t ori_z = 0;
			if ( witness[i] <= witness1[i] ){
				ori_z = 1;
			}
			uint64_t nm_z = (uint64_t)HIGH64(zLEQ[i].value);
			if (ori_z != nm_z){
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
