#pragma once

// #include "LinearLayer/fc-field.h"
// #include "LinearLayer/defines-HE.h"
// #include "emp-zk/emp-zk-arith/emp-zk-arith.h"
// #include "emp-zk/emp-zk-arith/polynomial.h"
// #include "emp-zk/emp-zk-math/ZKmath-functions.h"
// #include "emp-zk/emp-zk.h"
// #include "emp-zk/emp-vole/utility.h"
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

bool cdot = true;
// uint64_t verify = 1;

// int32_t bitlength = 44;
// int32_t bitlength = 59;
// uint64_t prime_mod = PLAINTEXT_MODULUS;
int party = 0;
// int num_threads = 8;
int port = 8000;
string address = "127.0.0.1";
// uint64_t num_rows = 1;
uint64_t common_dim = 1024;
uint64_t filter_precision = 15;
string benchmark;

// seal::Modulus mod(prime_mod);

// void Zk2Mpc(int party, NetIO *io, IntFp *x, uint64_t len, uint64_t *x_value, uint64_t *mac_M, uint64_t *delta, uint64_t *mac_K){
//   if(party==ALICE){
//     for(uint64_t i=0; i<len; i++){
//       x_value[i] = (uint64_t)HIGH64(x[i].value);
//       mac_M[i] = (uint64_t)LOW64(x[i].value);
//     }
//     io->send_data(x_value, len*sizeof(uint64_t));
//     io->send_data(mac_M, len*sizeof(uint64_t));
//     io->recv_data(delta, sizeof(uint64_t));
//     io->recv_data(mac_K, len*sizeof(uint64_t));
//     memset(x_value, 0, len*sizeof(uint64_t));
//     memset(mac_M, 0, len*sizeof(uint64_t));
//   }else{
//     *delta = (uint64_t)LOW64(((ZKFpExecVer<NetIO> *)(ZKFpExec::zk_exec))->ostriple->delta);
//     for(uint64_t i=0; i<len; i++){
//       mac_K[i] = (uint64_t)LOW64(x[i].value);
//     }
//     io->recv_data(x_value, len*sizeof(uint64_t));
//     io->recv_data(mac_M, len*sizeof(uint64_t));
//     io->send_data(delta, sizeof(uint64_t));
//     io->send_data(mac_K, len*sizeof(uint64_t));
//     memset(delta, 0, sizeof(uint64_t));
//     memset(mac_K, 0, len*sizeof(uint64_t));
//   }
// }

void LinearLayerFC(int party, NetIO *io, uint64_t common_dim) {

  uint64_t *input_c = new uint64_t[common_dim];
	PRG prg;
	memset(input_c, 0, common_dim*sizeof(uint64_t));
	if(party == ALICE){
		random_mod_p(prg, input_c, common_dim, prime_mod);
  	// for(int i = 0; i < common_dim; ++i){
    //   // input_c[i] %= prime_mod;
		// 	input_c[i] = 1%prime_mod;
		// }
	}
	IntFp *x = new IntFp[common_dim];
	for(uint64_t i=0; i<common_dim; i++){
		x[i] = IntFp(input_c[i], ALICE);
	}
  uint64_t *input_s = new uint64_t[common_dim];
	memset(input_c, 0, common_dim*sizeof(uint64_t));
	if(party == BOB){
		random_mod_p(prg, input_s, common_dim, prime_mod);
	}

  // client
  uint64_t *input_x = new uint64_t[common_dim];
  memset(input_x, 0, common_dim*sizeof(uint64_t));
  uint64_t *mac_M = new uint64_t[common_dim];
  memset(mac_M, 0, common_dim*sizeof(uint64_t));
  // server
  uint64_t mux = 0;
  uint64_t mac_key = 0;
  uint64_t *mac_K = new uint64_t[common_dim];
  memset(mac_K, 0, common_dim*sizeof(uint64_t));

  Zk2Mpc(party, io, x, common_dim, input_s, common_dim, input_x, mac_M, &mac_key, mac_K);
  
  FCField he_fc(party, io);
  uint64_t num_cols = 1;
  uint64_t num_rows = 1;

  //Setup Input objects
  vector<vector<uint64_t>> matrix;
  matrix.resize(num_rows, vector<uint64_t>(common_dim, 0));
  vector<vector<uint64_t>> input_share;
  input_share.resize(common_dim, vector<uint64_t>(num_cols, 0));
  vector<vector<uint64_t>> mac_M_vec;
  mac_M_vec.resize(common_dim, vector<uint64_t>(num_cols, 0));
  vector<vector<uint64_t>> mac_K_vec;
  mac_K_vec.resize(common_dim, vector<uint64_t>(num_cols, 0));

  if(party == BOB){
    //Prepare Dummy Inputs
    for(uint64_t i=0; i<common_dim; i++) {
      for(uint64_t j=0; j<num_cols; j++){
        input_share[i][j] = input_x[i];
        mac_M_vec[i][j] = mac_M[i];
        // std::cout << "input_share[" << i << "][" << j << "]:" << input_share[i][j] << std::endl;
      }
    }
  }else{
    mux = 0%2;
    for(uint64_t i=0; i<common_dim; i++) {
      for(uint64_t j=0; j<num_cols; j++){
        mac_K_vec[i][j] = mac_K[i];
      }
    }
    //Create input matrix
    for(uint64_t i=0; i< num_rows; i++) {
      random_mod_p(prg, matrix[i].data(), common_dim, prime_mod);
      // for(uint64_t j=0; j<common_dim; j++){
      //   matrix[i][j] = 1%prime_mod;
      //   // std::cout << "mac_input_share[" << i << "][" << j << "]:" << mac_input_share[i][j] << std::endl;
      // }
    }
  }

  uint64_t ss_res = 0, ss_mac_res = 0;
  uint64_t *ss_res_1 = new uint64_t[common_dim];
  uint64_t *ss_mac_res_1 = new uint64_t[common_dim];

  uint64_t comm_sent = 0;
  uint64_t start_comm = io->counter;
  auto start = clock_start();

  if(cdot){
    he_fc.vector_multiplication_large_no_share(num_rows, common_dim, num_cols, matrix, input_share, mac_M_vec, mac_K_vec, mac_key, 
                                              &ss_res, &ss_mac_res, seal::Modulus(prime_mod), verify, false);
  }else{
    he_fc.vector_bool_multiplication_large_no_share(num_rows, common_dim, num_cols, mux, input_share, mac_M_vec, mac_K_vec, mac_key, 
                                              ss_res_1, ss_mac_res_1, seal::Modulus(prime_mod), verify, false, wait_time);
  }

  long long t = time_from(start);
  total_time += t;

  cout << "######################Performance#######################" <<endl;
  cout<<"Time Taken: "<<total_time<<" us"<<endl;
  cout<<"Time Taken: "<<total_time/1000.0<<" ms"<<endl;
  cout<<"Time Taken: "<<total_time/1000000.0<<" s"<<endl;
  //Calculate Communication
  comm_sent = (io->counter-start_comm);
  cout<<"Sent Data (KB): "<<(comm_sent/1024.0)<<endl;
  cout<<"Sent Data (MB): "<<(comm_sent/1024.0/1024.0)<<endl;
  cout << "########################################################" <<endl;
  //LinearLayerFirstFC(he_fc, num_rows, common_dim);
}

void test_multiplication(int party, NetIO *io, uint64_t common_dim){
  // generate test data
  uint64_t *input_c = new uint64_t[common_dim];
	PRG prg;
	memset(input_c, 0, common_dim*sizeof(uint64_t));
	if(party == ALICE){
		random_mod_p(prg, input_c, common_dim, prime_mod);
  	for(int i = 0; i < common_dim; ++i){
      // input_c[i] %= prime_mod;
			input_c[i] = 1%prime_mod;
		}
	}
	IntFp *x = new IntFp[common_dim];
	for(uint64_t i=0; i<common_dim; i++){
		x[i] = IntFp(input_c[i], ALICE);
	}
  uint64_t *input_s = new uint64_t[common_dim];
	memset(input_s, 0, common_dim*sizeof(uint64_t));
  uint64_t mux = 0;
	if(party == BOB){
		random_mod_p(prg, input_s, common_dim, prime_mod);
    for(int i = 0; i < common_dim; ++i){
      // input_c[i] %= prime_mod;
			input_s[i] = 1%prime_mod;
		}
    mux = input_s[0]%2;
	}

  uint64_t ss_res = 0, ss_mac_res = 0;
  uint64_t *ss_res_1 = new uint64_t[common_dim];
  uint64_t *ss_mac_res_1 = new uint64_t[common_dim];

  he_fc.init(party, io);

  io->flush();

  uint64_t comm_sent = 0;
  uint64_t start_comm = io->counter;
  auto start = clock_start();

  if(cdot){
    vector_multiplication(party, io, x, common_dim, input_s, common_dim, &ss_res, &ss_mac_res); 
  }else{
    vector_bool_multiplication(party, io, x, common_dim, mux, ss_res_1, ss_mac_res_1);
  }

  long long t = time_from(start);
  total_time += t;

  cout << "######################Performance#######################" <<endl;
  cout<<"Time Taken: "<<total_time<<" us"<<endl;
  cout<<"Time Taken: "<<total_time/1000.0<<" ms"<<endl;
  cout<<"Time Taken: "<<total_time/1000000.0<<" s"<<endl;
  //Calculate Communication
  comm_sent = (io->counter-start_comm);
  cout<<"Sent Data (B): "<<(comm_sent)<<endl;
  cout<<"Sent Data (KB): "<<(comm_sent>>10)<<endl;
  cout<<"Sent Data (MB): "<<(comm_sent>>20)<<endl;
  cout << "########################################################" <<endl;
}

void parse_arguments(int argc, char**arg, int *party, int *port) {
  *party = atoi (arg[1]);
   address = arg[2];
	*port = atoi (arg[3]);

  if(argc >= 5) {
    common_dim = atoi (arg[4]);
  }

  if(argc >= 6) {
		verify = atoi (arg[5]);
	}

  if(argc >= 7) { // 1: test vector inner product, 0: test bool mult vector
		cdot = (bool)atoi (arg[6]);
	}
}

int main(int argc, char** argv){
  parse_arguments(argc, argv, &party, &port);
  cout<<"Executing FL Multiplication ..."<<endl;
  cout << "=====================Configuration======================" << endl;
  cout<<"Role: "<< party<<" - IP Address: "<< address <<" - Port: "<<port<<" - Bitlength: "<<bitlength<<endl;
  cout << "========================================================" << endl;

  BoolIO<NetIO> *ios[num_threads];
  for (int i = 0; i < num_threads; ++i)
		ios[i] = new BoolIO<NetIO>(new NetIO(party == ALICE ? nullptr : "127.0.0.1", port + i), party == ALICE);

  setup_zk_bool<BoolIO<NetIO>>(ios, num_threads, party);
	std::cout << "----------setup_zk_bool Finish ------------" << std::endl;
	setup_zk_arith<BoolIO<NetIO>>(ios, num_threads, party, false);
	std::cout << "----------setup_zk_arith Finish ------------" << std::endl;
	sync_zk_bool<BoolIO<NetIO>>();

  // LinearLayerFC(party, ios[0]->io, common_dim);
  test_multiplication(party, ios[0]->io, common_dim);

  finalize_zk_bool<BoolIO<NetIO>>();
	finalize_zk_arith<BoolIO<NetIO>>();

  for (int i = 0; i < num_threads; ++i)
	{
		delete ios[i]->io;
		delete ios[i];
	}
  return 0;
}
