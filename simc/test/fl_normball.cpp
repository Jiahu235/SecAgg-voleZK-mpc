#pragma once

// #include "LinearLayer/fc-field.h"
// #include "LinearLayer/defines-HE.h"
// #include "emp-zk/emp-zk-arith/emp-zk-arith.h"
// #include "emp-zk/emp-zk-arith/polynomial.h"
// #include "emp-zk/emp-zk-math/ZKmath-functions.h"
// #include "emp-zk/emp-zk.h"
// #include "emp-zk/emp-vole/utility.h"

#include <iostream>
#include "emp-sh2pc/emp-sh2pc.h"
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
// int num_threads = 8;

int party = 0;
int port = 8000;
string address = "127.0.0.1";

// uint64_t verify = 1;

// uint64_t bound_ln = 1 << (SCALE+FIXINT);
// uint64_t bound_l2 = 1 << 20;
uint64_t ncheck = 1 << 10;

bool ss_zero = true;    // let the sharing of z held by client be 0


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
// 	// std::cout << "Commit finished" << std::endl;

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
// 	// std::cout << "Check result1: " << r0 << endl;
// 	// std::cout << "Check result2: " << r1 << endl;

// 	// check ab_x in range [0, bd]
// 	LUTRangeIntFp *LUTRange = new LUTRangeIntFp(party);
// 	LUTRange->LUTRangeinit(bd);
// 	for(int i=0; i<len_x; i++){
// 	  	LUTRange->LUTRangeread(ab_x[i]);
// 	}
// 	delete LUTRange;
// 	// std::cout << "All checks finished! " << endl;
// }

// // z = vec(x) cdot vec(y), x and mac_x are shared by server and client, y is held by server
// void Ideal_vector_multiplication(int party, NetIO* io, uint64_t *ss_x, uint64_t *ss_mac_x, uint64_t len_x, 
// 									uint64_t *y, uint64_t len_y, 
// 									uint64_t *ss_z, uint64_t *ss_mac_z, uint64_t mac_delta)
// {
// 	assert(len_x == len_y);
// 	// std::cout << "---------- Ideal_vector_multiplication running ------------" << std::endl;
// 	uint64_t len_z = 1;  // length of z is 1
// 	if(party == ALICE){
// 		io->send_data(ss_x, len_x*sizeof(uint64_t));
// 		io->send_data(ss_mac_x, len_x*sizeof(uint64_t));
// 		if(ss_zero){
// 			std::memset(ss_z, 0, len_z*sizeof(uint64_t));
// 			std::memset(ss_mac_z, 0, len_z*sizeof(uint64_t));
// 		}else{
// 			io->recv_data(ss_z, len_z*sizeof(uint64_t));
// 			io->recv_data(ss_mac_z, len_z*sizeof(uint64_t));
// 		}
// 	}else{
// 		uint64_t *from_c_ss_x = new uint64_t[len_x];
// 		uint64_t *from_c_ss_mac_x = new uint64_t[len_x];
// 		io->recv_data(from_c_ss_x, len_x*sizeof(uint64_t));
// 		io->recv_data(from_c_ss_mac_x, len_x*sizeof(uint64_t));

// 		uint64_t *real_x = new uint64_t[len_x];
// 		uint64_t *real_mac_x = new uint64_t[len_x];
// 		for(uint64_t i=0; i<len_x; i++){
// 			real_x[i] = (from_c_ss_x[i] + ss_x[i])%prime_mod;
// 			real_mac_x[i] = (from_c_ss_mac_x[i] + ss_mac_x[i])%prime_mod;
// 			if(mod_mult(real_x[i], mac_delta, prime_mod) != real_mac_x[i]){
// 				error("Mac mult input check failed!\n");
// 			}
// 			*ss_z = (*ss_z + mod_mult(real_x[i], y[i], prime_mod))%prime_mod;
// 			*ss_mac_z = (*ss_mac_z + mod_mult(real_mac_x[i], y[i], prime_mod))%prime_mod;
// 			if(mod_mult(*ss_z, mac_delta, prime_mod) != *ss_mac_z){
// 				error("Mac mult output check failed!\n");
// 			}
// 		}
// 		if(!ss_zero){
// 			PRG prg;
// 			uint64_t to_c_ss_z;
// 			uint64_t to_c_ss_mac_z;
// 			random_mod_p(prg, &to_c_ss_z, len_z, prime_mod);
// 			random_mod_p(prg, &to_c_ss_mac_z, len_z, prime_mod);
// 			io->send_data(&to_c_ss_z, len_z*sizeof(uint64_t));
// 			io->send_data(&to_c_ss_mac_z, len_z*sizeof(uint64_t));
// 			*ss_z = (*ss_z + prime_mod - to_c_ss_z)%prime_mod;
// 			*ss_mac_z = (*ss_mac_z + prime_mod - to_c_ss_mac_z)%prime_mod;
// 		}
// 	}
// 	// std::cout << "---------- Ideal_vector_multiplication finish ------------" << std::endl;
// }

// void Ideal_vector_multiplication_1(int party, NetIO* io, 
// 									IntFp *x, uint64_t len_x, uint64_t *y, uint64_t len_y, 
// 									uint64_t *ss_z, uint64_t *ss_mac_z)
// {
// 	assert(len_x == len_y);
// 	// std::cout << "---------- Ideal_vector_multiplication running ------------" << std::endl;
// 	uint64_t len_z = 1;  // length of z is 1
	
// 	uint64_t *real_x = new uint64_t[len_x];
// 	uint64_t *real_mac_x = new uint64_t[len_x];
// 	uint64_t *mac_key_x = new uint64_t[len_x];
// 	memset(real_x, 0, len_x*sizeof(uint64_t));
// 	memset(real_mac_x, 0, len_x*sizeof(uint64_t));
// 	memset(mac_key_x, 0, len_x*sizeof(uint64_t));

// 	if(party == ALICE){
// 		for(uint64_t i=0; i< len_x; i++){
// 			real_x[i] = (uint64_t)HIGH64(x[i].value);
// 			real_mac_x[i] = (uint64_t)LOW64(x[i].value);
// 		}
// 		io->send_data(real_x, len_x*sizeof(uint64_t));
// 		io->send_data(real_mac_x, len_x*sizeof(uint64_t));
// 		if(ss_zero){
// 			std::memset(ss_z, 0, len_z*sizeof(uint64_t));
// 			std::memset(ss_mac_z, 0, len_z*sizeof(uint64_t));
// 		}else{
// 			io->recv_data(ss_z, len_z*sizeof(uint64_t));
// 			io->recv_data(ss_mac_z, len_z*sizeof(uint64_t));
// 		}
// 	}else{
// 		io->recv_data(real_x, len_x*sizeof(uint64_t));
// 		io->recv_data(real_mac_x, len_x*sizeof(uint64_t));

// 		uint64_t mac_delta = (uint64_t)LOW64(((ZKFpExecVer<NetIO> *)(ZKFpExec::zk_exec))->ostriple->delta);
// 		for(uint64_t i=0; i<len_x; i++){
// 			mac_key_x[i] = (uint64_t)LOW64(x[i].value);
// 			if((mod_mult(real_x[i], mac_delta, prime_mod) + mac_key_x[i])%prime_mod != real_mac_x[i]){
// 				error("Mac mult input check failed!\n");
// 			}
// 			uint64_t tmp = mod_mult(real_x[i], y[i], prime_mod);
// 			*ss_z = (*ss_z + tmp)%prime_mod;
// 			*ss_mac_z = (*ss_mac_z + mod_mult(tmp, mac_delta, prime_mod))%prime_mod;	
// 		}
// 		if(mod_mult(*ss_z, mac_delta, prime_mod) != *ss_mac_z){
// 			error("Mac mult output check failed!\n");
// 		}

// 		if(!ss_zero){
// 			PRG prg;
// 			uint64_t to_c_ss_z;
// 			uint64_t to_c_ss_mac_z;
// 			random_mod_p(prg, &to_c_ss_z, len_z, prime_mod);
// 			random_mod_p(prg, &to_c_ss_mac_z, len_z, prime_mod);
// 			io->send_data(&to_c_ss_z, len_z*sizeof(uint64_t));
// 			io->send_data(&to_c_ss_mac_z, len_z*sizeof(uint64_t));
// 			*ss_z = (*ss_z + prime_mod - to_c_ss_z)%prime_mod;
// 			*ss_mac_z = (*ss_mac_z + prime_mod - to_c_ss_mac_z)%prime_mod;
// 		}
// 	}
// 	// std::cout << "---------- Ideal_vector_multiplication finish ------------" << std::endl;
// }

// // z = (x > 0)-----> x < prime_mod/2
// void Ideal_vector_comparison(int party, NetIO* io, uint64_t *ss_x, uint64_t *ss_mac_x, uint64_t len_x,
// 								uint64_t *ss_z, uint64_t *ss_mac_z, uint64_t mac_delta)
// {
// 	// std::cout << "---------- Ideal_vector_comparison running ------------" << std::endl;
// 	uint64_t len_z = len_x;
// 	if(party == ALICE){
// 		io->send_data(ss_x, len_x*sizeof(uint64_t));
// 		io->send_data(ss_mac_x, len_x*sizeof(uint64_t));
// 		if(ss_zero){
// 			std::memset(ss_z, 0, len_z*sizeof(uint64_t));
// 			std::memset(ss_mac_z, 0, len_z*sizeof(uint64_t));
// 		}else{
// 			io->recv_data(ss_z, len_z*sizeof(uint64_t));
// 			io->recv_data(ss_mac_z, len_z*sizeof(uint64_t));
// 		}
// 	}else{
// 		uint64_t *from_c_ss_x = new uint64_t[len_x];
// 		uint64_t *from_c_ss_mac_x = new uint64_t[len_x];
// 		io->recv_data(from_c_ss_x, len_x*sizeof(uint64_t));
// 		io->recv_data(from_c_ss_mac_x, len_x*sizeof(uint64_t));

// 		uint64_t *real_x = new uint64_t[len_x];
// 		uint64_t *real_mac_x = new uint64_t[len_x];
// 		for(uint64_t i=0; i<len_x; i++){
// 			real_x[i] = (from_c_ss_x[i] + ss_x[i])%prime_mod;
// 			real_mac_x[i] = (from_c_ss_mac_x[i] + ss_mac_x[i])%prime_mod;
// 			// std::cout << "real_x[i]: " << real_x[i] << std::endl;
// 			// std::cout << "prime_mod-real_x[i]: " << prime_mod - real_x[i] << std::endl;
// 			// std::cout << "real_mac_x[i]: " << real_mac_x[i] << std::endl;
// 			// std::cout << "mac_delta: " << mac_delta << std::endl;
// 			if(mod_mult(real_x[i], mac_delta, prime_mod) != real_mac_x[i]){
// 				error("Mac comparison input check failed!\n");
// 			}
// 			ss_z[i] = (real_x[i] < (prime_mod - 1)/2) ? 0 : 1;
// 			ss_mac_z[i] = (real_x[i] < (prime_mod - 1)/2) ? 0 : mac_delta;
// 		}
// 		if(!ss_zero){
// 			PRG prg;
// 			uint64_t *to_c_ss_z = new uint64_t[len_z];
// 			uint64_t *to_c_ss_mac_z = new uint64_t[len_z];
// 			random_mod_p(prg, to_c_ss_z, len_z, prime_mod);
// 			random_mod_p(prg, to_c_ss_mac_z, len_z, prime_mod);
// 			io->send_data(to_c_ss_z, len_z*sizeof(uint64_t));
// 			io->send_data(to_c_ss_mac_z, len_z*sizeof(uint64_t));
// 			for(uint64_t i=0; i<len_z; i++){
// 				ss_z[i] = (ss_z[i] + prime_mod - to_c_ss_z[i])%prime_mod;
// 				ss_mac_z[i] = (ss_mac_z[i] + prime_mod - to_c_ss_mac_z[i])%prime_mod;
// 			}
// 		}
// 	}
// 	// std::cout << "---------- Ideal_vector_comparison finish ------------" << std::endl;
// }

// void NormBall(int party, NetIO* io, IntFp *x, uint64_t len_x, uint64_t *y, uint64_t len_y, uint64_t bd_ln, uint64_t bd_l2, uint64_t *ss_res, uint64_t *ss_mac_res)
// {
// 	// Server: BOB, Client: ALICE
// 	// [x] held by Client and Server, y held by Server(y[i] = 0 for client)
// 	// check l2_norm(vec(x) - vec(y)) < bd_l2 && ln_norm(vec(x)) < bd_ln
// 	assert(len_x == len_y);
// 	uint64_t len = len_x;

// 	std::cout << "---------- NormBall running ------------" << std::endl;

// 	// ln_norm(vec(x)) < bd_ln
// 	LnCheck(party, x, len_x, bd_ln);

// 	// compute h = x ^2 && commit h 
// 	uint64_t sum = 0;
// 	if(party == ALICE){
// 		for(uint64_t i=0; i<len; i++){
// 			sum += mod_mult(((uint64_t)HIGH64(x[i].value)), ((uint64_t)HIGH64(x[i].value)), prime_mod);
// 		}
// 	}
// 	sum %= prime_mod;
// 	IntFp h = IntFp(sum, ALICE);

// 	// verify h
// 	IntFp c_sum = IntFp(0, ALICE);
// 	for(uint64_t i=0; i<len; i++){
// 		c_sum = x[i] * x[i] + c_sum;
// 	}
// 	IntFp ver_h = h + c_sum.negate();
// 	ver_h.reveal_zero();


// 	// share h, x, delta*h, delta*x   // sharing of h, x, delta*x are not neccesary, only need delta*h
// 	// PRG prg;
// 	uint64_t ss_h = 0;  // secret sharing of h
// 	uint64_t ss_mac_h = 0;   // secret sharing of delta*h: = M_h for server; = -K_h for client
// 	// uint64_t *ss_x =  new uint64_t[len];  // secret sharing of x
// 	// uint64_t *ss_mac_x = new uint64_t[len];   // secret sharing of delta*x: = M_x for Server; = -K_x for Client
// 	uint64_t mac_delta = 0;  // delta held by server, for client is 0
// 	if(party == ALICE){
// 	// 	// generate sharings of h, x for server
// 	// 	uint64_t ss_h_to_s;
// 	// 	uint64_t *ss_x_to_s = new uint64_t[len];
// 	// 	if(ss_zero){
// 	// 		ss_h_to_s = 0;
// 	// 		memset(ss_x_to_s, 0, len*sizeof(uint64_t));
// 	// 	}else{
// 	// 		random_mod_p(prg, &ss_h_to_s, 1, prime_mod);
// 	// 		random_mod_p(prg, ss_x_to_s, len, prime_mod);
// 	// 	}
// 	// 	io->send_data(&ss_h_to_s, sizeof(uint64_t));
// 	// 	io->send_data(ss_x_to_s, len*sizeof(uint64_t));
// 	// 	// compute sharings of h, x for client
// 	// 	uint64_t real_h = (uint64_t)HIGH64(h.value);  // value of h
// 	// 	ss_h = (real_h + prime_mod - ss_h_to_s)%prime_mod;
// 	// 	uint64_t *real_x = new uint64_t[len];  // value of x
// 	// 	for(uint64_t i=0; i<len; i++){
// 	// 		real_x[i] = (uint64_t)HIGH64(x[i].value);
// 	// 		ss_x[i] = (real_x[i] + prime_mod - ss_x_to_s[i])%prime_mod;
// 	// 	}

// 		ss_h = (uint64_t)HIGH64(h.value);
// 		uint64_t mac_h = (uint64_t)LOW64(h.value);  // M_h
// 		ss_mac_h = mac_h; 
// 		// uint64_t *mac_x = new uint64_t[len];   // M_x
// 		// for(uint64_t i=0; i<len; i++){
// 		// 	mac_x[i] = (uint64_t)LOW64(x[i].value);
// 		// 	ss_mac_x[i] = mac_x[i];
// 		// }
// 	}else{
// 	// 	io->recv_data(&ss_h, sizeof(uint64_t));
// 	// 	io->recv_data(ss_x, len*sizeof(uint64_t));

// 		mac_delta = (uint64_t)LOW64(((ZKFpExecVer<NetIO> *)(ZKFpExec::zk_exec))->ostriple->delta);
// 		uint64_t mac_key_h = (uint64_t)LOW64(h.value);  // K_h
// 		ss_mac_h = (prime_mod - mac_key_h);
// 		// uint64_t *mac_key_x = new uint64_t[len];   // K_x
// 		// for(uint64_t i=0; i<len; i++){
// 		// 	mac_key_x[i] = (uint64_t)LOW64(x[i].value);
// 		// 	ss_mac_x[i] = (prime_mod - mac_key_x[i]);
// 		// }
// 	}
// 	// std::cout << "ss_h: " << ss_h << std::endl;
// 	// std::cout << "prime_mod-ss_h: " << prime_mod-ss_h << std::endl;

// 	// compute vec(x) cdot vec(y)
// 	uint64_t ss_z = 0, ss_mac_z = 0;
// 	// Ideal_vector_multiplication(party, io, ss_x, ss_mac_x, len_x, y, len_y, &ss_z, &ss_mac_z, mac_delta);
// 	Ideal_vector_multiplication_1(party, io, x, len_x, y, len_y, &ss_z, &ss_mac_z);
// 	// std::cout << "ss_z: " << ss_z << std::endl;
// 	// std::cout << "prime_mod-ss_z: " << prime_mod-ss_z << std::endl;
// 	// std::cout << "ss_mac_z: " << ss_mac_z << std::endl;

// 	// comparison: x^2 - 2xy > bd_l2 - y^2, where x^2 and 2xy are sharings, bd_l2 - y^2 can be computed by server
	
// 	uint64_t ss_tmp = (ss_h + prime_mod - mod_mult(2, ss_z, prime_mod))%prime_mod;
// 	uint64_t ss_mac_tmp = (ss_mac_h + prime_mod - mod_mult(2, ss_mac_z, prime_mod))%prime_mod;
// 	uint64_t sqrt_y = 0; //compute y^2
// 	if(party == BOB){
// 		for(uint64_t i=0; i<len; i++) sqrt_y = (sqrt_y + mod_mult(y[i], y[i], prime_mod))%prime_mod;
// 		// std::cout << "sqrt_y: " << sqrt_y << std::endl;
// 		// std::cout << "prime_mod-sqrt_y: " << prime_mod-sqrt_y << std::endl;
// 		uint64_t new_bd = (mod_mult(bd_l2, bd_l2, prime_mod) + prime_mod - sqrt_y)%prime_mod;
// 		ss_tmp = (ss_tmp + prime_mod - new_bd)%prime_mod;
// 		ss_mac_tmp = (ss_mac_tmp + prime_mod - mod_mult(new_bd, mac_delta, prime_mod))%prime_mod;
// 	}
// 	// std::cout << "ss_tmp: " << ss_tmp << std::endl;
// 	// std::cout << "prime_mod-ss_tmp: " << prime_mod-ss_tmp << std::endl;
// 	// std::cout << "ss_mac_tmp: " << ss_mac_tmp << std::endl;
// 	Ideal_vector_comparison(party, io, &ss_tmp, &ss_mac_tmp, 1, ss_res, ss_mac_res, mac_delta);
// 	// std::cout << "*ss_res: " << *ss_res << std::endl;
// 	// std::cout << "*ss_mac_res: " << *ss_mac_res << std::endl;

// 	std::cout << "---------- NormBall finish ------------" << std::endl;
// }


void test_NormBall(BoolIO<NetIO> **ios, int party)
{
	uint64_t *input_c = new uint64_t[ncheck];
	PRG prg;
	memset(input_c, 0, ncheck*sizeof(uint64_t));
	if(party == ALICE){
		random_mod_p(prg, input_c, ncheck, prime_mod);
  		for(int i = 0; i < ncheck; ++i){
			input_c[i] %= bound_ln;
			// if(input_c[i] <= (prime_mod-1)/2) std::cout << "inputs["<< i <<"]: " << input_c[i] << std::endl;
			// else std::cout << "inputs["<< i <<"]: " << int64_t(input_c[i] - prime_mod) << std::endl;
		}
	}

	uint64_t start_comm[num_threads];
	for(int i=0; i<num_threads; i++) {
		start_comm[i] = ios[i]->counter;
	}
	auto commit_start = clock_start();

	IntFp *x = new IntFp[ncheck];
	for(int i=0; i<ncheck; i++){
		x[i] = IntFp(input_c[i], ALICE);
	}

	ios[0]->flush();

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

	uint64_t *y = new uint64_t[ncheck];
	memset(y, 0, ncheck*sizeof(uint64_t));
	if(party == BOB){
		random_mod_p(prg, y, ncheck, prime_mod);
		for(int i = 0; i < ncheck; ++i){
			y[i] %= bound_ln;
			// if(y[i] <= (prime_mod-1)/2) std::cout << "y["<< i <<"]: " << y[i] << std::endl;
			// else std::cout << "y["<< i <<"]: " << int64_t(y[i] - prime_mod) << std::endl;
		}
	}

	uint64_t ss_res = 0, ss_mac_res = 0;

	// he_fc.init(party, ios[0]->io, ios, num_threads);
	// ios[0]->io->flush();

	// uint64_t com1 = ios[0]->io->counter;
	uint64_t com1[num_threads];
	for(int i=0; i<num_threads; i++) {
		com1[i] = ios[i]->io->counter;
	}
	auto start = clock_start();

	NormBall(party, ios[0]->io, x, ncheck, y, ncheck, bound_ln, bound_l2, &ss_res, &ss_mac_res, emp::delta_blocks);

	double time2 = time_from(start);

	uint64_t com2 = 0;
	for(int i=0; i<num_threads; i++) {
    	com2 += (ios[i]->io->counter-com1[i]);
  	}

	std::cout << "party: " << party << endl;
	cout<<"Time Taken: "<<time2<<" us"<<endl;
  	cout<<"Time Taken: "<<time2/1000.0<<" ms"<<endl;
  	cout<<"Time Taken: "<<time2/1000000.0<<" s"<<endl;

	double swap_time_1 = 0;
	double wait_time_1 = 0;
	if(party == ALICE){
		ios[0]->io->send_data(&swap_time, sizeof(double));
		ios[0]->io->send_data(&wait_time, sizeof(double));
		ios[0]->io->recv_data(&swap_time_1, sizeof(double));
		ios[0]->io->recv_data(&wait_time_1, sizeof(double));
	}else{
		ios[0]->io->recv_data(&swap_time_1, sizeof(double));
		ios[0]->io->recv_data(&wait_time_1, sizeof(double));
		ios[0]->io->send_data(&swap_time, sizeof(double));
		ios[0]->io->send_data(&wait_time, sizeof(double));
	}

	std::cout 
			// << "\nTime for wait: " << wait_time_1 << " us"  
			<< "\nTime for wait: " << wait_time_1 / 1000 << " ms"  
			<< "\nTime for wait: " << wait_time_1 / 1000000 << " s"  
			<< std::endl;

	double time4 = time2 - v_time - swap_time + swap_time_1;
	std::cout 
			// << "\nActual time except verbose: " << time4 << " us"  
			<< "\nActual time except verbose: " << time4 / 1000 << " ms"  
			<< "\nActual time except verbose: " << time4 / 1000000 << " s"  
			<< std::endl;

	double time5 = time4 - wait_time_1;
	std::cout 
			// << "\nActual time except verbose and wait time: " << time5 << " us"  
			<< "\nActual time except verbose and wait time: " << time5 / 1000 << " ms"  
			<< "\nActual time except verbose and wait time: " << time5 / 1000000 << " s"  
			<< std::endl;

  	//Calculate Communication
  	// uint64_t com2 = comm(ios) - com1;
	// uint64_t com2 = ios[0]->io->counter - com1;
  	cout<<"Sent Data (B): "<<(com2)<<endl;
  	cout<<"Sent Data (KB): "<<(com2/1024.0)<<endl;
  	cout<<"Sent Data (MB): "<<(com2/1024.0/1024.0)<<endl;
	
	uint64_t swap_comm_1 = 0;
	if(party == ALICE){
		ios[0]->io->send_data(&swap_comm, sizeof(uint64_t));
		ios[0]->io->recv_data(&swap_comm_1, sizeof(uint64_t));
	}else{
		ios[0]->io->recv_data(&swap_comm_1, sizeof(uint64_t));
		ios[0]->io->send_data(&swap_comm, sizeof(uint64_t));
	}

	uint64_t com4 = com2 - v_comm - swap_comm + swap_comm_1;
	// std::cout << "Actual communication except verbose (B): " << com4 << std::endl;
	std::cout << "\nActual communication except verbose (KB): " << com4 / 1024.0 << std::endl;
	std::cout << "Actual communication except verbose (MB): " << com4 / 1024.0 / 1024.0 << std::endl;

	if(verify){
		if(party == ALICE){
			ios[0]->io->send_data(input_c, ncheck*sizeof(uint64_t));
			ios[0]->io->send_data(&ss_res, sizeof(uint64_t));
			ios[0]->io->send_data(&ss_mac_res, sizeof(uint64_t));
		}else{
			ios[0]->io->recv_data(input_c, ncheck*sizeof(uint64_t));
			uint64_t from_c_ss_res, from_c_ss_mac_res;
			ios[0]->io->recv_data(&from_c_ss_res, sizeof(uint64_t));
			ios[0]->io->recv_data(&from_c_ss_mac_res, sizeof(uint64_t));
			uint64_t real_res = (from_c_ss_res + ss_res)%prime_mod;
			uint64_t real_mac_res = (from_c_ss_mac_res + ss_mac_res)%prime_mod;
			uint64_t mac_delta = (uint64_t)LOW64(((ZKFpExecVer<NetIO> *)(ZKFpExec::zk_exec))->ostriple->delta);
			if(mod_mult(real_res, mac_delta, prime_mod) != real_mac_res){
				error("Mac check failed!\n");
			}
			uint64_t sqrt_x = 0;
			for(uint64_t i=0; i<ncheck; i++){
				sqrt_x = (sqrt_x + mod_mult((input_c[i]+prime_mod-y[i])%prime_mod, (input_c[i]+prime_mod-y[i])%prime_mod, prime_mod))%prime_mod;
			}
			std::cout << "sqrt(x-y): " << sqrt_x << std::endl;
			std::cout << "NormBall results: " << real_res << std::endl;
			if(((sqrt_x < bound_l2*bound_l2)? 1 : 0) == real_res) std::cout << "NormBall results: " << real_res << std::endl;
			else error("Correctness check failed!\n");
		}
	}
}


void parse_arguments(int argc, char**arg) {
	party = atoi (arg[1]);
	address = arg[2];
	port = atoi (arg[3]);

	if(argc >= 5) {
		verify = atoi (arg[4]);
	}

  	if(argc >= 6) {
    	ncheck = atoi (arg[5]);
  	}

	if(argc >= 7) {
    	bound_l2 = atoi (arg[6]);
  	}

	if(argc >= 8) {
    	bound_ln = atoi (arg[7]);
  	}
}


int main(int argc, char **argv)
{
	parse_arguments(argc, argv);
	BoolIO<NetIO> *ios[num_threads];
	for (int i = 0; i < num_threads; ++i)
		ios[i] = new BoolIO<NetIO>(new NetIO(party == ALICE ? nullptr : "127.0.0.1", port + i), party == ALICE);

	std::cout << "\n"
			  << "------------ Exp_exact zero-knowledge proof test ------------\n"
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

	he_fc.init(party, ios[0]->io, ios, num_threads);
	ios[0]->io->flush();

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

	test_NormBall(ios, party);

	endComputation(party);
	std::cout << "-------------- finish test -----------------" << std::endl;

	finalize_zk_bool<BoolIO<NetIO>>();
	finalize_zk_arith<BoolIO<NetIO>>();

	for (int i = 0; i < num_threads; ++i)
	{
		delete ios[i]->io;
		delete ios[i];
	}
	return 0;
}

