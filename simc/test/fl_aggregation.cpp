// #pragma once

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

// #define HELP_SCALE 0   //把实数嵌入域时，放大更多倍，减少误差

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

// uint64_t bound_cos = 1 << (SCALE-3);
// uint64_t bound_ln = 1 << 20;
// uint64_t allow_error = 1 < 1;
// uint64_t ncheck = 1 << 10;

uint64_t num_client = 2;
uint64_t dimension = 1 << 10;

bool ss_zero = true;  // let the sharing of z held by client be 0

int FilterType = 1;

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
// 	for(uint64_t i=0; i<len_x; i++){
// 	  	LUTRange->LUTRangeread(ab_x[i]);
// 	}
// 	delete LUTRange;
// 	// std::cout << "All checks finished! " << endl;
// }

// void Ideal_L2Check(int party, IntFp *x, uint64_t len_x, uint64_t up_bd, uint64_t low_bd=0)
// {
// 	uint64_t *real_x = new uint64_t[len_x];
// 	memset(real_x, 0, len_x*sizeof(uint64_t));
// 	if(party == ALICE){
// 		uint64_t sum = 0;
// 		for(uint64_t i=0; i< len_x; i++){
// 			real_x[i] = (uint64_t)HIGH64(x[i].value);
// 			sum += real_x[i];
// 		}
// 		if((sum <= low_bd*low_bd) && (sum >= up_bd*up_bd)) error("L2Check failed!");
// 	}
// }

// z = vec(x) cdot vec(y), x and mac_x are shared by server and client, y is held by server
// void Ideal_vector_multiplication(int party, NetIO* io, uint64_t *ss_x, uint64_t *ss_mac_x, uint64_t len_x, 
// 									uint64_t *y, uint64_t len_y, 
// 									uint64_t *ss_z, uint64_t *ss_mac_z, uint64_t mac_delta)
// {
// 	assert(len_x == len_y);
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

// void Ideal_vector_multiplication_2(int party, NetIO* io, 
// 									IntFp *x, uint64_t len_x, uint64_t y, uint64_t mac_delta,
// 									uint64_t *ss_z, uint64_t *ss_mac_z)
// {
// 	// 需要实现一个bool乘以vector，实际实现时对原本的乘法进行简化即可
// 	// [x] held by client and server, y held by server, compute <xy> and <delta*xy>
// 	uint64_t *input_x = new uint64_t[len_x];
// 	memset(input_x, 0, len_x*sizeof(uint64_t));
// 	if(party == ALICE){
// 		for(uint64_t i=0; i<len_x; i++){
// 			input_x[i] = (uint64_t)HIGH64(x[i].value);
// 		}
// 		io->send_data(input_x, len_x*sizeof(uint64_t));
// 		memset(ss_z, 0, len_x*sizeof(uint64_t));
// 		memset(ss_mac_z, 0, len_x*sizeof(uint64_t));
// 	}else{
// 		io->recv_data(input_x, len_x*sizeof(uint64_t));
// 		if(y == 0){ // y is either 0 or 1
// 			memset(ss_z, 0, len_x*sizeof(uint64_t));
// 			memset(ss_mac_z, 0, len_x*sizeof(uint64_t));
// 		}else{
// 			memcpy(ss_z, input_x, len_x*sizeof(uint64_t));
// 			for(uint64_t i=0; i<len_x; i++){
// 				ss_mac_z[i] = mod_mult(input_x[i], mac_delta, prime_mod);
// 			}
// 		}
// 	}
// }

// // z = (x > 0)-----> x < prime_mod/2
// void Ideal_vector_comparison(int party, NetIO* io, uint64_t *ss_x, uint64_t *ss_mac_x, uint64_t len_x,
// 								uint64_t *ss_z, uint64_t *ss_mac_z, uint64_t mac_delta)
// {
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
// }

// void CosSim(int party, NetIO* io, IntFp *x, uint64_t len_x, uint64_t *y, uint64_t len_y, uint64_t bd, uint64_t *ss_res, uint64_t *ss_mac_res)
// {
// 	// Server: BOB, Client: ALICE
// 	// [x] held by Client and Server, y held by Server (y[i] = 0 for client)
// 	// check cos(vec(x), vec(y)) > bd
// 	assert(len_x == len_y);
// 	uint64_t len = len_x;

// 	// ln_norm(vec(x)) < bound_ln
// 	LnCheck(party, x, len_x, bound_ln);

// 	// for client: compute normalized vector h, h = x / ||x||_2 
// 	uint64_t *input_h = new uint64_t[len];
// 	memset(input_h, 0, len*sizeof(uint64_t));
// 	uint64_t *input = new uint64_t[len];
// 	double *real_input = new double[len];
// 	double input_cdot_input = 0;
// 	double l2_norm_input = 0;
// 	uint64_t l2_norm_input_field = 0;
// 	if(party == ALICE){
// 		for(uint64_t i=0; i<len; i++){
// 			input[i] = ((uint64_t)HIGH64(x[i].value))%prime_mod;
// 			real_input[i] = Field2Real(input[i], SCALE);
// 			input_cdot_input += real_input[i] * real_input[i];
// 		}
// 		l2_norm_input = pow(input_cdot_input, 0.5);
// 		for(int i=0; i<len; i++){
// 			input_h[i] = Real2Field(real_input[i]/l2_norm_input, SCALE+HELP_SCALE);
// 		}
// 		l2_norm_input_field = Real2Field(l2_norm_input, SCALE+HELP_SCALE);
// 	}
// 	IntFp *h = new IntFp[len];
// 	for(int i=0; i<len; i++){
// 		h[i] = IntFp(input_h[i], ALICE);
// 	}
// 	IntFp c_l2_norm_input_field = IntFp(l2_norm_input_field, ALICE);

// 	// verify h
// 	IntFp *ver_h = new IntFp[len];
// 	for(uint64_t i=0; i<len; i++){
// 		ver_h[i] = x[i].negate() + h[i] * c_l2_norm_input_field;
// 	}
// 	// batch_reveal_check_zero(ver_h, len);
	

// 	// Check ||h||_2 = 1
// 	Ideal_L2Check(party, h, len, (1<<(SCALE+HELP_SCALE))+allow_error, (1<<(SCALE+HELP_SCALE))-allow_error);
// 	// L2Check(party, h, len, (1<<(SCALE+HELP_SCALE))+allow_error, (1<<(SCALE+HELP_SCALE))-allow_error);

	
// 	// for server: compute normalized vector g, g = y / ||y||_2
// 	uint64_t *g = new uint64_t[len];
// 	memset(g, 0, len*sizeof(uint64_t));
// 	double *real_y = new double[len];
// 	double input_cdot_input_y = 0;
// 	double l2_norm_y = 0;
// 	if(party == BOB){
// 		for(uint64_t i=0; i<len; i++){
// 			real_y[i] = Field2Real(y[i], SCALE);
// 			input_cdot_input_y += real_y[i] * real_y[i];
// 		}
// 		l2_norm_y = pow(input_cdot_input_y, 0.5);
// 		for(int i=0; i<len; i++){
// 			g[i] = Real2Field(real_y[i]/l2_norm_y, SCALE+HELP_SCALE);
// 		}
// 	}

// 	// share h, delta*h  //not neccesary
// 	// PRG prg;
// 	// uint64_t *ss_h =  new uint64_t[len];  // secret sharing of h
// 	// memset(ss_h, 0, len*sizeof(uint64_t));
// 	// uint64_t *ss_mac_h = new uint64_t[len];   // secret sharing of delta*h: = M_h for Server; = -K_h for Client
// 	// memset(ss_mac_h, 0, len*sizeof(uint64_t));
// 	// uint64_t mac_delta = 0;  // delta held by server, for client is 0
// 	// if(party == ALICE){
// 	// 	// generate sharings of h for server
// 	// 	uint64_t *ss_h_to_s = new uint64_t[len];
// 	// 	random_mod_p(prg, ss_h_to_s, len, prime_mod);
// 	// 	if(ss_zero){
// 	// 		memset(ss_h_to_s, 0, len*sizeof(uint64_t));
// 	// 	}else{
// 	// 		random_mod_p(prg, ss_h_to_s, len, prime_mod);
// 	// 	}
// 	// 	io->send_data(ss_h_to_s, len*sizeof(uint64_t));

// 	// 	// compute sharings of h for client
// 	// 	uint64_t *real_h = new uint64_t[len];  // value of h
// 	// 	for(uint64_t i=0; i<len; i++){
// 	// 		real_h[i] = (uint64_t)HIGH64(h[i].value);
// 	// 		ss_h[i] = (real_h[i] + prime_mod - ss_h_to_s[i])%prime_mod;
// 	// 	}
		 
// 	// 	// sharings of delta*h
// 	// 	uint64_t *mac_h = new uint64_t[len];   // M_h
// 	// 	for(uint64_t i=0; i<len; i++){
// 	// 		mac_h[i] = (uint64_t)LOW64(h[i].value);
// 	// 		ss_mac_h[i] = mac_h[i];
// 	// 	}
// 	// }else{
// 	// 	io->recv_data(ss_h, len*sizeof(uint64_t));

// 	// 	mac_delta = (uint64_t)LOW64(((ZKFpExecVer<NetIO> *)(ZKFpExec::zk_exec))->ostriple->delta);
// 	// 	uint64_t *mac_key_h = new uint64_t[len];   // K_h
// 	// 	for(uint64_t i=0; i<len; i++){
// 	// 		mac_key_h[i] = (uint64_t)LOW64(h[i].value);
// 	// 		ss_mac_h[i] = (prime_mod - mac_key_h[i])%prime_mod;
// 	// 	}
// 	// }

// 	// compute vec(x) cdot vec(y)
// 	uint64_t ss_z = 0, ss_mac_z = 0;
// 	// Ideal_vector_multiplication(party, io, ss_h, ss_mac_h, len, g, len, &ss_z, &ss_mac_z, mac_delta);  //不做截断，小数位数翻倍
// 	Ideal_vector_multiplication_1(party, io, h, len, g, len, &ss_z, &ss_mac_z);			//不做截断，小数位数翻倍
// 	// std::cout << "ss_z: " << ss_z << std::endl;
// 	// std::cout << "ss_mac_z: " << ss_mac_z << std::endl;
// 	// Ideal_vector_truncate(party, io, &ss_z, &ss_mac_z, 1, &ss_z, &ss_mac_z, SCALE, mac_delta);

// 	// comparison: x^2 - 2xy > bd_l2 - y^2, where x^2 and 2xy are sharings, bd_l2 - y^2 can be computed by server
// 	uint64_t mac_delta = 0;
// 	uint64_t ss_tmp = mod_mult(ss_z, prime_mod-1, prime_mod);  // -ss_z
// 	uint64_t ss_mac_tmp = mod_mult(ss_mac_z, prime_mod-1, prime_mod);  // -ss_mac_z
// 	uint64_t new_bd = mod_mult(bd, 1<<(SCALE+2*HELP_SCALE), prime_mod);  //由于没有做截断，需要把bd放大，左移SCALE位，再进行比较
// 	if(party == BOB){
// 		mac_delta = (uint64_t)LOW64(((ZKFpExecVer<NetIO> *)(ZKFpExec::zk_exec))->ostriple->delta);
// 		ss_tmp = (ss_tmp + new_bd)%prime_mod;
// 		ss_mac_tmp = (ss_mac_tmp + mod_mult(new_bd, mac_delta, prime_mod))%prime_mod;
// 	}
// 	// std::cout << "ss_tmp: " << ss_tmp << std::endl;
// 	// std::cout << "ss_mac_tmp: " << ss_mac_tmp << std::endl;
// 	Ideal_vector_comparison(party, io, &ss_tmp, &ss_mac_tmp, 1, ss_res, ss_mac_res, mac_delta);
// 	// std::cout << "*ss_res: " << *ss_res << std::endl;
// 	// std::cout << "*ss_mac_res: " << *ss_mac_res << std::endl;
// }

// void GradFilter(int party, NetIO* io, IntFp *x, uint64_t len_x, uint64_t *y, uint64_t len_y, uint64_t bd, uint64_t *ss_res, uint64_t *ss_mac_res){
// 	// a simple test: if x[0]%2==0, then set *ss_res=1, *ss_mac_res=delta; otherwise *ss_res=0, *ss_mac_res=0.
// 	uint64_t mac_delta = 0;
// 	if(party == BOB){
// 		mac_delta = (uint64_t)LOW64(((ZKFpExecVer<NetIO> *)(ZKFpExec::zk_exec))->ostriple->delta);
// 		io->send_data(&mac_delta, sizeof(uint64_t));
// 		*ss_res = 0;
// 		*ss_mac_res = 0;
// 	}else{
// 		io->recv_data(&mac_delta, sizeof(uint64_t));
// 		*ss_res = (((uint64_t)HIGH64(x[0].value))%2==0) ? 1 : 0;
// 		*ss_mac_res = (((uint64_t)HIGH64(x[0].value))%2==0) ? mac_delta : 0;
// 	}
// }

// void Aggregation(int party, NetIO* io, uint64_t **x, uint64_t num_client, uint64_t len_x, uint64_t *y, uint64_t len_y, uint64_t *z){
// 	// x are the gradients held by clients, x[i] is held by client i, num_client is the number of clients
// 	// y is the gradient held by server
// 	// len_x, len_y is the dimension of gradients
// 	// z is the result of the aggregation
// 	assert(len_x == len_y);
// 	uint64_t len = len_x;

// 	PRG prg;
// 	uint64_t **rand_for_ss_zero = new uint64_t*[num_client];
// 	uint64_t **rand_for_ss_mac_zero = new uint64_t*[num_client];
// 	for(uint64_t i=0; i<num_client; i++){
// 		rand_for_ss_zero[i] = new uint64_t[len];
// 		rand_for_ss_mac_zero[i] = new uint64_t[len];
// 		random_mod_p(prg, rand_for_ss_zero[i], len, prime_mod);
// 		random_mod_p(prg, rand_for_ss_mac_zero[i], len, prime_mod);
// 	}

// 	IntFp **input_x = new IntFp*[num_client];
// 	uint64_t mac_delta = 0;

// 	uint64_t *weight_x = new uint64_t[num_client];
// 	memset(weight_x, 0, num_client*sizeof(uint64_t));
// 	uint64_t *mac_weight_x = new uint64_t[num_client];
// 	memset(mac_weight_x, 0, num_client*sizeof(uint64_t));

// 	uint64_t **weighted_gradient = new uint64_t*[num_client];
// 	uint64_t **mac_weighted_gradient = new uint64_t*[num_client];
// 	uint64_t **from_c_weighted_gradient = new uint64_t*[num_client];
// 	uint64_t **from_c_mac_weighted_gradient = new uint64_t*[num_client];

// 	uint64_t *aggregated_result = new uint64_t[len];
// 	memset(aggregated_result, 0, len*sizeof(uint64_t));
// 	uint64_t *mac_aggregated_result = new uint64_t[len];
// 	memset(mac_aggregated_result, 0, len*sizeof(uint64_t));

// 	for(uint64_t i=0; i<num_client; i++){
// 		// commit
// 		input_x[i] = new IntFp[len];
// 		for(uint64_t j=0; j<len; j++){
// 			input_x[i][j] = IntFp(x[i][j], ALICE);
// 		}
// 		if(party == BOB){
// 			mac_delta = (uint64_t)LOW64(((ZKFpExecVer<NetIO> *)(ZKFpExec::zk_exec))->ostriple->delta);
// 		}

// 		// compute the weight of x
// 		uint64_t tmp_weight=0, tmp_mac_weight=0;
// 		GradFilter(party, io, input_x[i], len, y, len, bound_cos, &tmp_weight, &tmp_mac_weight);

// 		uint64_t real_weight=0, real_mac_weight=0;
// 		if(party==ALICE){
// 			io->send_data(&tmp_weight, sizeof(uint64_t));
// 			io->send_data(&tmp_mac_weight, sizeof(uint64_t));
// 		}else{
// 			uint64_t weight_from_c=0, mac_weight_from_c=0;
// 			io->recv_data(&weight_from_c, sizeof(uint64_t));
// 			io->recv_data(&mac_weight_from_c, sizeof(uint64_t));
// 			real_weight = (weight_from_c + tmp_weight)%prime_mod;
// 			real_mac_weight = (mac_weight_from_c + tmp_mac_weight)%prime_mod;
// 			if(mod_mult(mac_delta, real_weight, prime_mod) != real_mac_weight){  // client i modify its weight share, exclude it
// 				error("Abort! There are malicious clients!\n");
// 				// real_weight = 0;
// 				// real_mac_weight = 0;
// 			}
// 			weight_x[i] = real_weight;
// 			mac_weight_x[i] = real_mac_weight;
// 		}

// 		// compute weighted gradients: <weight_x * x> and <delta*weight_x * x>
// 		weighted_gradient[i] = new uint64_t[len];
// 		memset(weighted_gradient[i], 0, len*sizeof(uint64_t));
// 		mac_weighted_gradient[i] = new uint64_t[len];
// 		memset(mac_weighted_gradient[i], 0, len*sizeof(uint64_t));
// 		Ideal_vector_multiplication_2(party, io, input_x[i], len, real_weight, mac_delta, weighted_gradient[i], mac_weighted_gradient[i]);

// 		// clients mask weighted gradients and send them to server
// 		if(party == ALICE){
// 			for(uint64_t j=0; j<len; j++){
// 				weighted_gradient[i][j] = (weighted_gradient[i][j] + rand_for_ss_zero[i][j] + prime_mod - rand_for_ss_zero[(i+1)%num_client][j])%prime_mod;
// 				mac_weighted_gradient[i][j] = (mac_weighted_gradient[i][j] + rand_for_ss_mac_zero[i][j] + prime_mod - rand_for_ss_mac_zero[(i+1)%num_client][j])%prime_mod;
// 			}
// 			io->send_data(weighted_gradient[i], len*sizeof(uint64_t));
// 			io->send_data(mac_weighted_gradient[i], len*sizeof(uint64_t));
// 		}else{
// 			from_c_weighted_gradient[i] = new uint64_t[len];
// 			from_c_mac_weighted_gradient[i] = new uint64_t[len];
// 			io->recv_data(from_c_weighted_gradient[i], len*sizeof(uint64_t));
// 			io->recv_data(from_c_mac_weighted_gradient[i], len*sizeof(uint64_t));
// 		}
// 	}
// 	if(party == BOB){
// 		uint64_t *from_c_aggregation = new uint64_t[len];
// 		uint64_t *from_c_mac_aggregation = new uint64_t[len];
// 		memset(from_c_aggregation, 0, len*sizeof(uint64_t));
// 		memset(from_c_mac_aggregation, 0, len*sizeof(uint64_t));

// 		uint64_t *from_s_aggregation = new uint64_t[len];
// 		uint64_t *from_s_mac_aggregation = new uint64_t[len];
// 		memset(from_s_aggregation, 0, len*sizeof(uint64_t));
// 		memset(from_s_mac_aggregation, 0, len*sizeof(uint64_t));

// 		// sum(from_c_weighted_gradient), sum(from_c_mac_weighted_gradient)
// 		for(uint64_t i=0; i<num_client; i++){
// 			for(uint64_t j=0; j<len; j++){
// 				from_c_aggregation[j] = (from_c_aggregation[j] + from_c_weighted_gradient[i][j])%prime_mod;
// 				from_c_mac_aggregation[j] = (from_c_mac_aggregation[j] + from_c_mac_weighted_gradient[i][j])%prime_mod;
// 				from_s_aggregation[j] = (from_s_aggregation[j] + weighted_gradient[i][j])%prime_mod;
// 				from_s_mac_aggregation[j] = (from_s_mac_aggregation[j] + mac_weighted_gradient[i][j])%prime_mod;
// 			}
// 		}

// 		// verify: delta * aggregation == mac_aggregation
// 		uint64_t ctr = 0;
// 		for(uint64_t i=0; i<len; i++){
// 			aggregated_result[i] = (from_c_aggregation[i]+from_s_aggregation[i])%prime_mod;
// 			mac_aggregated_result[i] = (from_c_mac_aggregation[i]+from_s_mac_aggregation[i])%prime_mod;
// 			if(mod_mult(aggregated_result[i], mac_delta, prime_mod) == mac_aggregated_result[i]){
// 				ctr++;
// 			}
// 		}
// 		if(ctr < len){
// 			error("Check failed! Some clients modify their sharings of weighted gradients!\n");
// 		}
// 		// send aggregation result to clients?
// 	}
// 	memcpy(z, aggregated_result, len*sizeof(uint64_t));
// }


void test_Aggregation(BoolIO<NetIO> **ios, int party)
{
	// generate clients' gradients
	uint64_t **input_c = new uint64_t*[num_client];
	double **real_input_c = new double*[num_client];
	for(uint64_t i=0; i<num_client; i++) {
		input_c[i] = new uint64_t[dimension];
		memset(input_c[i], 0, dimension*sizeof(uint64_t));
		real_input_c[i] = new double[dimension];
	}
	PRG prg;
	if(party == ALICE){
		for(uint64_t i=0; i<num_client; i++){
			random_mod_p(prg, input_c[i], dimension, prime_mod);
			#pragma omp parallel for num_threads(num_threads) schedule(static)
  			for(uint64_t j = 0; j < dimension; ++j){
				input_c[i][j] %= 1<<(FIXINT+SCALE);
				// if(j%8192<6114) input_c[i][j] = 0;  // 根据实际梯度
				// input_c[i][j] = (input_c[i][j]%2) ? input_c[i][j] : (prime_mod-input_c[i][j])%prime_mod;
				real_input_c[i][j] = Field2Real(input_c[i][j], SCALE);
				// if(inputs[i] <= (PR-1)/2) std::cout << "inputs["<< i <<"]: " << inputs[i] << std::endl;
				// else std::cout << "inputs["<< i <<"]: " << int64_t(inputs[i] - PR) << std::endl;
			}
		}
	}

	// generate server's gradients
	uint64_t *y = new uint64_t[dimension];
	double *real_input_s = new double[dimension];
	memset(y, 0, dimension*sizeof(uint64_t));
	if(party == BOB){
		random_mod_p(prg, y, dimension, prime_mod);
		#pragma omp parallel for num_threads(num_threads) schedule(static)
		for(uint64_t i = 0; i < dimension; ++i){
			y[i] %= 1<<(FIXINT+SCALE);
			// if(i%8192<6114) y[i] = 0; // 根据实际梯度
			// y[i] = (y[i]%2==0) ? y[i] : (prime_mod-y[i])%prime_mod;
			real_input_s[i] = Field2Real(y[i], SCALE);
		}
	}

	uint64_t *aggregation_result = new uint64_t[dimension];
	memset(aggregation_result, 0, dimension*sizeof(uint64_t));

	uint64_t he_com1 = ios[0]->io->counter;
	auto he_start = clock_start();
	he_fc.init(party, ios[0]->io, ios, num_threads);
	ios[0]->io->flush();
	double he_time = time_from(he_start);
	uint64_t he_com2 = ios[0]->io->counter - he_com1;

	std::cout 
			// << "\nTime for HE init: " << he_time << " us"
			<< "\nTime for HE init: " << he_time / 1000 << " ms"
			<< "\nTime for HE init: " << he_time / 1000000 << " s"
			<< std::endl;
	std::cout << "HE init communication (B): " << he_com2 << std::endl;
	std::cout << "HE init communication (KB): " << he_com2 / 1024.0 << std::endl;
	std::cout << "HE init communication (MB): " << he_com2 / 1024.0 / 1024.0 << std::endl;
	std::cout << "-------------- start test -----------------" << std::endl;

	// uint64_t total_com1 = 0;
	// for(int i=0; i<num_threads; i++) {
    // 	total_com1 += (ioArr[i]->counter-start_comm[i]);
  	// }
	uint64_t com1 = ios[0]->io->counter;
	auto start = clock_start();

	Aggregation(party, ios[0]->io, input_c, num_client, dimension, y, dimension, aggregation_result, emp::delta_blocks, FilterType);

	double time2 = time_from(start);

	std::cout << "party: " << party 
				// << "\nAll time: " << time2 << " us"  
				<< "\nAll time: " << time2 / 1000 << " ms"  
				<< "\nAll time: " << time2 / 1000000 << " s"  
				<< std::endl;

	double time3 = time2 - v_time;
	std::cout 
			// << "\nTime except verbose: " << time3 << " us"  
			<< "\nTime except verbose: " << time3 / 1000 << " ms"  
			<< "\nTime except verbose: " << time3 / 1000000 << " s"  
			<< std::endl;

	std::cout 
			// << "\nSwap Time: " << swap_time << " us"  
			<< "\nSwap Time: " << swap_time / 1000 << " ms"  
			<< "\nSwap Time: " << swap_time / 1000000 << " s"  
			<< std::endl;
	
	std::cout 
			// << "\nTime for ln: " << ln_time << " us"  
			<< "\nTime for ln: " << ln_time / 1000 << " ms"  
			<< "\nTime for ln: " << ln_time / 1000000 << " s"  
			<< std::endl;

	std::cout 
			// << "\nTime for l2: " << l2_time << " us"  
			<< "\nTime for l2: " << l2_time / 1000 << " ms"  
			<< "\nTime for l2: " << l2_time / 1000000 << " s"  
			<< std::endl;
	
	double swap_time_1 = 0;
	double mult_time_1 = 0;
	double comp_time_1 = 0;
	double wait_time_1 = 0;
	// swap_time 中包含了执行乘法过程中的等待时间，应当减去这段时间
	// swap_time -= wait_time;
	if(party == ALICE){
		ios[0]->io->send_data(&swap_time, sizeof(double));
		ios[0]->io->send_data(&mult_time, sizeof(double));
		ios[0]->io->send_data(&comp_time, sizeof(double));
		ios[0]->io->send_data(&wait_time, sizeof(double));

		ios[0]->io->recv_data(&swap_time_1, sizeof(double));
		ios[0]->io->recv_data(&mult_time_1, sizeof(double));
		ios[0]->io->recv_data(&comp_time_1, sizeof(double));
		ios[0]->io->recv_data(&wait_time_1, sizeof(double));
	}else{
		ios[0]->io->recv_data(&swap_time_1, sizeof(double));
		ios[0]->io->recv_data(&mult_time_1, sizeof(double));
		ios[0]->io->recv_data(&comp_time_1, sizeof(double));
		ios[0]->io->recv_data(&wait_time_1, sizeof(double));

		ios[0]->io->send_data(&swap_time, sizeof(double));
		ios[0]->io->send_data(&mult_time, sizeof(double));
		ios[0]->io->send_data(&comp_time, sizeof(double));
		ios[0]->io->send_data(&wait_time, sizeof(double));
	}

	std::cout 
			// << "\nTime for mult: " << mult_time_1 << " us"  
			<< "\nTime for mult: " << mult_time_1 / 1000 << " ms"  
			<< "\nTime for mult: " << mult_time_1 / 1000000 << " s"  
			<< std::endl;
	
	std::cout 
			// << "\nTime for comp: " << comp_time_1 << " us"  
			<< "\nTime for comp: " << comp_time_1 / 1000 << " ms"  
			<< "\nTime for comp: " << comp_time_1 / 1000000 << " s"  
			<< std::endl;

	std::cout 
			// << "\nTime for wait: " << wait_time_1 << " us"  
			<< "\nTime for wait: " << wait_time_1 / 1000 << " ms"  
			<< "\nTime for wait: " << wait_time_1 / 1000000 << " s"  
			<< std::endl;

	std::cout 
			// << "\nTime for mult-wait: " << (mult_time_1 - wait_time_1) << " us"  
			<< "\nTime for mult-wait: " << (mult_time_1 - wait_time_1) / 1000 << " ms"  
			<< "\nTime for mult-wait: " << (mult_time_1 - wait_time_1) / 1000000 << " s"  
			<< std::endl;

	double time4 = time3 - swap_time + swap_time_1;
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

	uint64_t com2 = ios[0]->io->counter - com1;
	// std::cout << "All communication (B): " << com2 << std::endl;
	std::cout << "\nAll communication (KB): " << com2 / 1024.0 << std::endl;
	std::cout << "All communication (MB): " << com2 / 1024.0 / 1024.0 << std::endl;

	uint64_t com3 = com2 - v_comm;
	// std::cout << "Communication except verbose (B): " << com3 << std::endl;
	std::cout << "\nCommunication except verbose (KB): " << com3 / 1024.0 << std::endl;
	std::cout << "Communication except verbose (MB): " << com3 / 1024.0 / 1024.0 << std::endl;

	// std::cout << "Ln communication (B): " << ln_comm << std::endl;
	std::cout << "\nLn communication (KB): " << ln_comm / 1024.0 << std::endl;
	std::cout << "Ln communication except (MB): " << ln_comm / 1024.0 / 1024.0 << std::endl;

	// std::cout << "L2 communication (B): " << l2_comm << std::endl;
	std::cout << "\nL2 communication (KB): " << l2_comm / 1024.0 << std::endl;
	std::cout << "L2 communication except (MB): " << l2_comm / 1024.0 / 1024.0 << std::endl;

	uint64_t swap_comm_1 = 0;
	uint64_t mult_comm_1 = 0;
	uint64_t comp_comm_1 = 0;
	if(party == ALICE){
		ios[0]->io->send_data(&swap_comm, sizeof(uint64_t));
		ios[0]->io->send_data(&mult_comm, sizeof(uint64_t));
		ios[0]->io->send_data(&comp_comm, sizeof(uint64_t));

		ios[0]->io->recv_data(&swap_comm_1, sizeof(uint64_t));
		ios[0]->io->recv_data(&mult_comm_1, sizeof(uint64_t));
		ios[0]->io->recv_data(&comp_comm_1, sizeof(uint64_t));
	}else{
		ios[0]->io->recv_data(&swap_comm_1, sizeof(uint64_t));
		ios[0]->io->recv_data(&mult_comm_1, sizeof(uint64_t));
		ios[0]->io->recv_data(&comp_comm_1, sizeof(uint64_t));

		ios[0]->io->send_data(&swap_comm, sizeof(uint64_t));
		ios[0]->io->send_data(&mult_comm, sizeof(uint64_t));
		ios[0]->io->send_data(&comp_comm, sizeof(uint64_t));
	}

	// std::cout << "Mult communication (B): " << mult_comm_1 << std::endl;
	std::cout << "\nMult communication (KB): " << mult_comm_1 / 1024.0 << std::endl;
	std::cout << "Mult communication except (MB): " << mult_comm_1 / 1024.0 / 1024.0 << std::endl;

	// std::cout << "Comp communication (B): " << com4 << std::endl;
	std::cout << "\nComp communication (KB): " << comp_comm_1 / 1024.0 << std::endl;
	std::cout << "Comp communication except (MB): " << comp_comm_1 / 1024.0 / 1024.0 << std::endl;

	uint64_t com4 = com3 - swap_comm + swap_comm_1;
	// std::cout << "Actual communication except verbose (B): " << com4 << std::endl;
	std::cout << "\nActual communication except verbose (KB): " << com4 / 1024.0 << std::endl;
	std::cout << "Actual communication except verbose (MB): " << com4 / 1024.0 / 1024.0 << std::endl;
	std::cout << "Actual communication except verbose (GB): " << com4 / 1024.0 / 1024.0 / 1024.0 << std::endl;

	if(verify){
		if(party == ALICE){
			for(uint64_t i=0; i<num_client; i++){
				ios[0]->io->send_data(input_c[i], dimension*sizeof(uint64_t));
			}
		}else{
			for(uint64_t i=0; i<num_client; i++){
				ios[0]->io->recv_data(input_c[i], dimension*sizeof(uint64_t));
			}

			// check the result in the field
			uint64_t *w = new uint64_t[num_client];  // weight of clients
			memset(w, 0, num_client*sizeof(uint64_t));
			uint64_t *result = new uint64_t[dimension];
			memset(result, 0, dimension*sizeof(uint64_t));
			for(uint64_t i=0; i<num_client; i++){
				// if(input_c[i][0]%2==0){ // According to GradFilter
				// 	w[i] = 1;
				// }else{
				// 	w[i] = 0;
				// }
				// According to Normball
				uint64_t sqrt_x = 0;
				for(uint64_t j=0; j<dimension; j++){
					sqrt_x = (sqrt_x + mod_mult((input_c[i][j]+prime_mod-y[j])%prime_mod, (input_c[i][j]+prime_mod-y[j])%prime_mod, prime_mod))%prime_mod;
				}
				w[i] = (sqrt_x < bound_l2*bound_l2);

				// compute aggregation result
				for(uint64_t j=0; j<dimension; j++){
					result[j] = (w[i]==1) ? (result[j] + input_c[i][j])%prime_mod : result[j];
				}
			}
			uint64_t ctr = 0;
			for(uint64_t i=0; i<dimension; i++){
				if(result[i] == aggregation_result[i])
					ctr++;
			}
			if(ctr < dimension) error("Result fault!\n");

			// check the result in real number
			double *real_result = new double[dimension];
			for(uint64_t i = 0; i < dimension; ++i) real_result[i] = 0;
			for(uint64_t i=0; i<num_client; i++){
  				for(uint64_t j = 0; j < dimension; ++j){
					real_input_c[i][j] = Field2Real(input_c[i][j], SCALE);
					real_result[j] = (w[i]==1) ? (real_result[j]+real_input_c[i][j]) : real_result[j];
				}
			}
			uint64_t ctr_real = 0;
			for(uint64_t i=0; i<dimension; i++){
				if(Field2Real(aggregation_result[i], SCALE) == real_result[i])
					ctr_real++;
			}
			if(ctr_real < dimension) error("Real Result fault!\n");
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
    	dimension = atoi (arg[5]);
  	}

	if(argc >= 7) {
    	num_client = atoi (arg[6]);
  	}

	if(argc >= 8) {
		FilterType = atoi (arg[7]);
	}

	if(argc >= 9) {
    	bound_l2 = atoi (arg[8]);
  	}

	

	// if(argc >= 7) {
    // 	bound_cos = atoi (arg[6]);
  	// }

	// if(argc >= 8) {
    // 	allow_error = atoi (arg[7]);
  	// }

	// if(argc >= 9) {
    	// bound_ln = atoi (arg[8]);
  	// }
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
	
	// setup_semi_honest_mult(ios[0], party, 0);
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
			<< "\nTime for offline: " << offline_time / 1000.0 << " ms"
			<< "\nTime for offline: " << offline_time / 1000000.0 << " s"
			<< std::endl;
	std::cout << "Offline communication (B): " << offline_comm << std::endl;
	std::cout << "Offline communication (KB): " << offline_comm / 1024.0 << std::endl;
	std::cout << "Offline communication (MB): " << offline_comm / 1024.0 / 1024.0 << std::endl;
	std::cout << "-------------- start test -----------------" << std::endl;

	startComputation(party);
	
	test_Aggregation(ios, party);

	endComputation(party);

	std::cout << "-------------- finish test -----------------" << std::endl;
	
	finalize_zk_bool<BoolIO<NetIO>>();
	finalize_zk_arith<BoolIO<NetIO>>();
	// finalize_semi_honest();

	for (int i = 0; i < num_threads; ++i)
	{
		delete ios[i]->io;
		delete ios[i];
	}
	return 0;
}

