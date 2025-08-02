// #pragma once

// #include "LinearLayer/fc-field.h"
// #include "LinearLayer/defines-HE.h"
// #include "emp-zk/emp-zk-arith/emp-zk-arith.h"
// #include "emp-zk/emp-zk-arith/polynomial.h"
// #include "emp-zk/emp-zk-math/ZKmath-functions.h"
// #include "emp-zk/emp-zk.h"
// #include "emp-zk/emp-vole/utility.h"

#include <iostream>
#include "fl_utils.h"

using namespace std;
using namespace emp;
using namespace seal;

// enum neural_net {
//   NONE,
//   MINIONN,
//   CIFAR10
// };
// neural_net choice_nn;
// neural_net def_nn = NONE;

long long total_time = 0;

// int32_t bitlength = 44;
// int32_t bitlength = 59;
// uint64_t prime_mod = PLAINTEXT_MODULUS;
int party = 0;
// int num_threads = 8;
int port = 8000;
string address = "127.0.0.1";
string benchmark;

uint64_t bound = 65535;  //2^32
uint64_t ncheck = 1 << 10;


// uint64_t comm(BoolIO<NetIO> **ios)
// {
// 	uint64_t c = 0;
// 	for (int i = 0; i < num_threads; ++i)
// 		c += ios[i]->counter;
// 	return c;
// }

// void LnCheck(int party, IntFp *x, uint64_t len_x, uint64_t bd)
// {
// 	// parse x = s * hx, s为0或1, hx为x的绝对值
// 	uint64_t *s = new uint64_t[len_x];
// 	uint64_t *hx = new uint64_t[len_x];
// 	if(party == ALICE){
// 		for(uint64_t i=0; i<len_x; i++){
// 			uint64_t *v = (uint64_t*)&(x[i].value);
// 			// std::cout << "x["<< i <<"].value: " << v[1] << std::endl;
// 			if(v[1] <= (PR-1)/2){
// 				s[i] = 1;
// 				hx[i] = v[1];
// 			}else{
// 				s[i] = 0;
// 				hx[i] = PR - v[1];
// 			}
// 			// std::cout << "s["<< i <<"]: " << s[i] << std::endl;
// 			// std::cout << "hx["<< i <<"]: " << hx[i] << std::endl;
// 		}
// 	}

// 	// commit s and hx, obtain sign and ab_x
// 	IntFp *sign = new IntFp[len_x];
// 	IntFp *ab_x = new IntFp[len_x];
// 	for(uint64_t i=0; i<len_x; i++){
// 		sign[i] = IntFp(s[i], ALICE);
// 		ab_x[i] = IntFp(hx[i], ALICE);
// 	}
// 	std::cout << "Commit finished" << std::endl;

// 	// check x = sign *ab_x and sign*(sign-1)=0
// 	IntFp *check_mult = new IntFp[len_x];
// 	IntFp *check_sign = new IntFp[len_x];
// 	for(uint64_t i=0; i<len_x; i++){
// 		// std::cout << "sign["<< i <<"]: " << ((uint64_t*)&(sign[i].value))[1] << std::endl;
// 		// std::cout << "ab_x["<< i <<"]: " << ((uint64_t*)&(ab_x[i].value))[1] << std::endl;
// 		check_mult[i] = x[i].negate() + ((sign[i] * 2) + (PR - 1)) * ab_x[i];
// 		check_sign[i] = (sign[i] + (PR - 1)) * sign[i];
// 		// std::cout << "check_mult["<< i <<"]: " << ((uint64_t*)&(check_mult[i].value))[1] << std::endl;
// 		// std::cout << "check_sign["<< i <<"]: " << ((uint64_t*)&(check_sign[i].value))[1] << std::endl;
// 	}
// 	// uint64_t *zero_a = new uint64_t[len_x];
// 	// memset(zero_a, 0, len_x * sizeof(uint64_t));
// 	// // zero_a[0] += 4;
// 	// batch_reveal_check(check_mult, zero_a, len_x);
// 	// batch_reveal_check(check_sign, zero_a, len_x);
// 	bool r0 = batch_reveal_check_zero(check_mult, len_x);
// 	bool r1 = batch_reveal_check_zero(check_sign, len_x);
// 	std::cout << "Check result1: " << r0 << endl;
// 	std::cout << "Check result2: " << r1 << endl;

// 	// check ab_x in range [0, bd]
// 	LUTRangeIntFp *LUTRange = new LUTRangeIntFp(party);
// 	LUTRange->LUTRangeinit(bd);
// 	for(int i=0; i<len_x; i++){
// 	  	LUTRange->LUTRangeread(ab_x[i]);
// 	}
// 	delete LUTRange;
// 	std::cout << "All checks finished! " << endl;
// }

void test_LnCheck(BoolIO<NetIO> **ios, int party)
{
	// generate test data
	// std::random_device rd;
  	// std::mt19937_64 eng(rd());
  	// std::uniform_int_distribution<uint64_t> distr;
	uint64_t *inputs = new uint64_t[ncheck];
  	for(int i = 0; i < ncheck; ++i){
    	// inputs[i] = distr(eng)%PR;
		// inputs[i] = ((i%2 == 0) ? i : PR-i)%PR;
		inputs[i] = i%bound;
		// if(inputs[i] <= (PR-1)/2) std::cout << "inputs["<< i <<"]: " << inputs[i] << std::endl;
		// else std::cout << "inputs["<< i <<"]: " << int64_t(inputs[i] - PR) << std::endl;
	}

	uint64_t start_comm[num_threads];
	for(int i=0; i<num_threads; i++) {
		start_comm[i] = ios[i]->counter;
	}
	auto commit_start = clock_start();

	IntFp *x = new IntFp[ncheck];
	for(int i=0; i<ncheck; i++){
		x[i] = IntFp(inputs[i], ALICE);
	}

	ios[0]->io->flush();

	double commit_time = time_from(commit_start);
	uint64_t commit_comm = 0;
	for(int i=0; i<num_threads; i++) {
    	commit_comm += (ios[i]->counter-start_comm[i]);
  	}
	std::cout 
			// << "\nTime for commit: " << commit_time << " us"
			<< "\nTime for commit: " << commit_time / 1000 << " ms"
			<< "\nTime for commit: " << commit_time / 1000000 << " s"
			<< std::endl;
	std::cout << "commit communication (B): " << commit_comm << std::endl;
	std::cout << "commit communication (KB): " << commit_comm / 1024.0 << std::endl;
	std::cout << "commit communication (MB): " << commit_comm / 1024.0 / 1024.0 << std::endl;

	uint64_t com1[num_threads];
	for(int i=0; i<num_threads; i++) {
		com1[i] = ios[i]->counter;
	}
	// uint64_t com1 = ios[0]->io->counter;
	auto start = clock_start();

	LnCheck(party, x, ncheck, bound);

	double time2 = time_from(start);

	std::cout << "party: " << party << endl;
	cout<<"Time Taken: "<<time2<<" us"<<endl;
  	cout<<"Time Taken: "<<time2/1000.0<<" ms"<<endl;
  	cout<<"Time Taken: "<<time2/1000000.0<<" s"<<endl;
  	//Calculate Communication
  	// uint64_t com2 = comm(ios) - com1;
	uint64_t com2 = 0;
	for(int i=0; i<num_threads; i++) {
    	com2 += (ios[i]->counter-com1[i]);
  	}
	// uint64_t com2 = ios[0]->io->counter - com1;
  	cout<<"Sent Data (B): "<<(com2)<<endl;
  	cout<<"Sent Data (KB): "<<(com2/1024.0)<<endl;
  	cout<<"Sent Data (MB): "<<(com2/1024.0/1024.0)<<endl;
	
	// std::cout << "party: " << party << 
	// 			"\ntime: " << time2 / 1000 << " ms"  << endl;

	// uint64_t com2 = comm(ios) - com1;
	// std::cout << "communication (KB): " << com2 / 1024.0 << std::endl;
}


void parse_arguments(int argc, char**arg, int *party, int *port) {
	*party = atoi (arg[1]);
	address = arg[2];
	*port = atoi (arg[3]);

	if(argc >= 5) {
		ncheck = atoi (arg[4]);
	}

  	if(argc >= 6) {
    	bound = atoi (arg[5]);
  	}
}


int main(int argc, char **argv)
{
	parse_arguments(argc, argv, &party, &port);
	BoolIO<NetIO> *ios[num_threads];
	for (int i = 0; i < num_threads; ++i)
		ios[i] = new BoolIO<NetIO>(new NetIO(party == ALICE ? nullptr : "127.0.0.1", port + i), party == ALICE);

	std::cout << std::endl
			  << "------------ Exp_exact zero-knowledge proof test ------------" << std::endl
			  << std::endl;

	uint64_t start_comm[num_threads];
	for(int i=0; i<num_threads; i++) {
		start_comm[i] = ios[i]->counter;
	}
	auto start = clock_start();
  	
	setup_zk_bool<BoolIO<NetIO>>(ios, num_threads, party);
	std::cout << "----------setup_zk_bool Finish ------------" << std::endl;
	setup_zk_arith<BoolIO<NetIO>>(ios, num_threads, party, false);
	std::cout << "----------setup_zk_arith Finish ------------" << std::endl;

	sync_zk_bool<BoolIO<NetIO>>();

	double offline_time = time_from(start);
	uint64_t offline_comm = 0;
	for(int i=0; i<num_threads; i++) {
    	offline_comm += (ios[i]->counter-start_comm[i]);
  	}

	std::cout 
			// << "\nTime for offline: " << offline_time << " us"
			<< "\nTime for offline: " << offline_time / 1000 << " ms"
			<< "\nTime for offline: " << offline_time / 1000000 << " s"
			<< std::endl;
	std::cout << "Offline communication (B): " << offline_comm << std::endl;
	std::cout << "Offline communication (KB): " << offline_comm / 1024.0 << std::endl;
	std::cout << "Offline communication (MB): " << offline_comm / 1024.0 / 1024.0 << std::endl;
	std::cout << "-------------- start test -----------------" << std::endl;

	startComputation(party);

	test_LnCheck(ios, party);

	endComputation(party);
	cout << "-------------- finish test -----------------" << endl;

	finalize_zk_bool<BoolIO<NetIO>>();
	finalize_zk_arith<BoolIO<NetIO>>();

	for (int i = 0; i < num_threads; ++i)
	{
		delete ios[i]->io;
		delete ios[i];
	}
	return 0;
}

