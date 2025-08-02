#include "emp-sh2pc/emp-sh2pc.h"
#include <cmath>

// #include "seal/util/uintarith.h"
// #include "seal/util/uintarithsmallmod.h"
#include <thread>
#include "fl_utils.h"

#define MAX_THREADS 8
using namespace emp;
using namespace std;

// int num_threads = 1;

//Slackoverflow Code For bit-wise shift
// #define SHL128(v, n) \
// ({ \
//     __m128i v1, v2; \
//  \
//     if ((n) >= 64) \
//     { \
//         v1 = _mm_slli_si128(v, 8); \
//         v1 = _mm_slli_epi64(v1, (n) - 64); \
//     } \
//     else \
//     { \
//         v1 = _mm_slli_epi64(v, n); \
//         v2 = _mm_slli_si128(v, 8); \
//         v2 = _mm_srli_epi64(v2, 64 - (n)); \
//         v1 = _mm_or_si128(v1, v2); \
//     } \
//     v1; \
// })


uint64_t start_comm[MAX_THREADS];
uint64_t comm_sent = 0;
NetIO *ioArr[MAX_THREADS];
// uint64_t prime_mod = 17592060215297;
// uint64_t prime_mod = 576460752034988033;
// seal::Modulus mod(prime_mod);
int port = 32000;
// int32_t l = 59;    //44->59
uint64_t def_nrelu = 1<<20;  
string address;
uint64_t mac_key = 0;
PRG prg;
string benchmark;

// uint64_t verify = 1;

// uint64_t mod_shift(uint64_t a, uint64_t b, uint64_t prime_mod) {
//     __m128i temp=makeBlock(0, 0), stemp=makeBlock(0, 0);
//     memcpy(&temp, &a, 8);
//     stemp = SHL128(temp, b);

//     uint64_t input[2];
//     input[0] = stemp[0];
//     input[1] = stemp[1];

//     uint64_t result = seal::util::barrett_reduce_128(input, mod);

//     return result;
// }

// uint64_t mod_mult(uint64_t a, uint64_t b) {
//   unsigned long long temp_result[2];
//   seal::util::multiply_uint64(a, b, temp_result);

//   /*uint64_t input[2];
//   input[0] = res[0];
//   input[1] = res[1];*/
//   uint64_t result = seal::util::barrett_reduce_128(temp_result, mod);
//   return result;
// }


// //Referred SCI OT repo's logic to pack ot messages
// void pack_decryption_table(uint64_t *pack_table, uint64_t *ciphertexts, int pack_size, int batch_size, int bitlen) {
//   uint64_t beg_idx = 0;
//   uint64_t end_idx = 0;
//   uint64_t beg_blk = 0;
//   uint64_t end_blk = 0;
//   uint64_t temp_blk = 0;
//   uint64_t mask = (1ULL << bitlen) - 1;
//   uint64_t pack_blk_size = 64;

//   if (bitlen == 64)
//     mask = -1;

//   for (int i = 0; i < pack_size; i++) {
//     pack_table[i] = 0;
//   }

//   for (int i = 0; i < batch_size; i++) {
//     beg_idx = i * bitlen;
//     end_idx = beg_idx + bitlen;
//     end_idx -= 1;
//     beg_blk = beg_idx / pack_blk_size;
//     end_blk = end_idx / pack_blk_size;

//     if (beg_blk == end_blk) {
//       pack_table[beg_blk] ^= (ciphertexts[i] & mask) << (beg_idx % pack_blk_size);
//     } else {
//       temp_blk = (ciphertexts[i] & mask);
//       pack_table[beg_blk] ^= (temp_blk) << (beg_idx % pack_blk_size);
//       pack_table[end_blk] ^= (temp_blk) >> (pack_blk_size - (beg_idx % pack_blk_size));
//     }
//   }
// }

// //Referred SCI OT repo's logic to unpack ot messages
// void unpack_decryption_table(uint64_t *pack_table, uint64_t *ciphertexts, int pack_size, int batch_size, int bitlen) {
//   uint64_t beg_idx = 0;
//   uint64_t end_idx = 0;
//   uint64_t beg_blk = 0;
//   uint64_t end_blk = 0;
//   uint64_t temp_blk = 0;
//   uint64_t mask = (1ULL << bitlen) - 1;
//   uint64_t pack_blk_size = 64;

//   for (int i = 0; i < batch_size; i++) {
//     beg_idx = i * bitlen;
//     end_idx = beg_idx + bitlen - 1;
//     beg_blk = beg_idx / pack_blk_size;
//     end_blk = end_idx / pack_blk_size;

//     if (beg_blk == end_blk) {
//       ciphertexts[i] = (pack_table[beg_blk] >> (beg_idx % pack_blk_size)) & mask;
//     } else {
//       ciphertexts[i] = 0;
//       ciphertexts[i] ^= (pack_table[beg_blk] >> (beg_idx % pack_blk_size));
//       ciphertexts[i] ^= (pack_table[end_blk] << (pack_blk_size - (beg_idx % pack_blk_size)));
//       ciphertexts[i] = ciphertexts[i] & mask;
//     }
//   }
// }

// void create_ciphertexts(Integer *garbled_data, block label_delta, uint64_t *ciphertexts, uint64_t* server_shares, int bitlen, uint64_t nrelu, uint64_t alpha, int l_idx, bool apply_prg) {
//   uint64_t delta_int;
//   memcpy(&delta_int, &label_delta[l_idx], 8);

//   uint64_t mask = (1ULL << bitlen) - 1;
//   block seed, label_block_0, label_block_1;
//   uint64_t label_temp_0, label_temp_1;
//   uint64_t **random_val = (uint64_t **)malloc(nrelu*sizeof(uint64_t*));
//   uint8_t pnp, cpnp;
//   for(uint64_t i=0; i<nrelu; i++) {
//     random_val[i] = (uint64_t *)malloc(bitlen*sizeof(uint64_t));
//   }

//   for(uint64_t i=0; i<nrelu; i++) {
//     server_shares[i] = 0;
//     for(int j=0; j<bitlen; j++) {
//       if(apply_prg) {
//         memcpy(&seed, &garbled_data[i].bits[j].bit, 16);
//         PRG prg0(&seed);
//         prg0.random_data(&label_block_0, 16);
//         seed = garbled_data[i].bits[j].bit^label_delta;
//         PRG prg1(&seed);
//         prg1.random_data(&label_block_1, 16);

//         memcpy(&label_temp_0, &label_block_0[l_idx], 8);
//         memcpy(&label_temp_1, &label_block_1[l_idx], 8);

//       } else {
//         memcpy(&label_temp_0, &garbled_data[i].bits[j].bit[l_idx], 8);
//         label_temp_1 = label_temp_0^delta_int;
//       }

//       PRG prg;
//       prg.random_data(&random_val[i][j], 8);
//       random_val[i][j] %= prime_mod;
//       // random_val[i][j] = 0%prime_mod;
//       pnp = (garbled_data[i].bits[j].bit[0]) & 1;
//       cpnp = 1 - pnp;
//       ciphertexts[(i*bitlen+j)*2+pnp] = (random_val[i][j])^(label_temp_0 & mask);
//       ciphertexts[(i*bitlen+j)*2+cpnp] = ((random_val[i][j]+alpha)%prime_mod)^(label_temp_1 & mask);
//       server_shares[i] = (server_shares[i] + mod_shift(random_val[i][j],j,prime_mod))%prime_mod;
//     }
//     server_shares[i] = (prime_mod - server_shares[i])%prime_mod;
//   }
// }

// void decrypt_ciphertexts(Integer *garbled_data, uint64_t *ciphertexts, uint64_t* client_shares, int bitlen, uint64_t nrelu, int l_idx, bool apply_prg) {
//   uint64_t label_temp;
//   block label_block;
//   uint8_t pnp;
//   uint64_t random_val;

//   uint64_t mask = (1ULL << bitlen) - 1;

//   for(uint64_t i=0; i<nrelu; i++) {
//     client_shares[i] = 0;
//     for(int j=0; j< bitlen; j++) {
//       if(apply_prg) {
//         PRG prg(&garbled_data[i].bits[j].bit);
//         prg.random_data(&label_block, 16);

//         memcpy(&label_temp, &label_block[l_idx], 8);
//       } else {
//         memcpy(&label_temp, &garbled_data[i].bits[j].bit[l_idx], 8);
//       }

//       pnp = (garbled_data[i].bits[j].bit[0]) & 1;
//       random_val = ciphertexts[(i*bitlen+j)*2+pnp]^(label_temp & mask);
//       // std::cout << "random_val[" << j << "]:" << random_val << std::endl;
//       client_shares[i] = (client_shares[i] + mod_shift(random_val,j,prime_mod))%prime_mod;
//     }
//   }
// }

// void comparison(int party, int tid, NetIO* io, uint64_t* inputs, uint64_t nrelu, int bitlen, uint64_t* ip_ss, uint64_t* op_ss, uint64_t* op_mss, uint64_t mac_key) {
//   //Public prime values
//   Integer p(bitlen, prime_mod, PUBLIC);
//   Integer p_mod2(bitlen, prime_mod/2, PUBLIC);
//   Integer zero(bitlen, 0, PUBLIC);

//   //Assign Inputs
//   Integer *X = new Integer[nrelu];
//   for(uint64_t i = 0; i < nrelu; ++i)
//     X[i] = Integer(bitlen, inputs[i], ALICE);
//   Integer *Y = new Integer[nrelu];
//   for(uint64_t i = 0; i < nrelu; ++i)
//     Y[i] = Integer(bitlen, inputs[i], BOB);

//   Integer *S = new Integer[nrelu];
//   Integer *U = new Integer[nrelu];
//   Integer *T = new Integer[nrelu];

//   //Check if Bob's share is < p
//   Bit res[nrelu];
//   for(uint64_t i=0; i < nrelu; ++i)
//     res[i] = Y[i] > p;

//   for(uint64_t i=0; i < nrelu; ++i) {
//     //Perform mod p
//     Integer s0 = X[i];
//     //s0.resize(s0.size()+1);

//     Integer s1 = Y[i];
//    //s1.resize(s1.size()+1);

//     Integer sum = s0 + s1;

//     Integer mod_p_val = sum - p;

//     Bit borrow_bit = mod_p_val[mod_p_val.size()-1];

//     Integer s = mod_p_val.select(borrow_bit, sum);

//     S[i] = s;

//     //Perform Comparison
//     Integer p2_minus_s = s - p_mod2;

//     Bit is_negative = p2_minus_s[p2_minus_s.size()-1];
//     vector<Bit> bit_res;
//     bit_res.push_back(is_negative);
//     Integer res = Integer(bit_res);

//     // Integer res = Integer(&is_negative);  //the result of comparison is 1 bit

//     T[i] = res;
//   }

//   int pack_size = ceil(nrelu*bitlen*bitlen*2.0/(8*sizeof(uint64_t)));
//   int pack_size_1 = ceil(nrelu*1*bitlen*2.0/(8*sizeof(uint64_t)));  //the result of comparison is 1 bit
//   int batch_size = nrelu*bitlen*2;
//   int batch_size_1 = nrelu*1*2;  //the result of comparison is 1 bit

//   uint64_t *ip_cts = (uint64_t *)malloc(nrelu*bitlen*2*sizeof(uint64_t));

//   uint64_t *op_cts = (uint64_t *)malloc(nrelu*1*2*sizeof(uint64_t));

//   uint64_t *op_mcts = (uint64_t *)malloc(nrelu*1*2*sizeof(uint64_t));

//   uint64_t *ip_pack_table = (uint64_t *)malloc(pack_size*sizeof(uint64_t));
//   uint64_t *op_pack_table = (uint64_t *)malloc(pack_size_1*sizeof(uint64_t));
//   uint64_t *opm_pack_table = (uint64_t *)malloc(pack_size_1*sizeof(uint64_t));

//   if(party == ALICE) {

//     create_ciphertexts(S, delta_blocks[tid], ip_cts, ip_ss, bitlen, nrelu, mac_key, 1, true);
//     create_ciphertexts(T, delta_blocks[tid], op_cts, op_ss, 1, nrelu, 1, 0, false);
//     create_ciphertexts(T, delta_blocks[tid], op_mcts, op_mss, 1, nrelu, mac_key, 1, false);

//     pack_decryption_table(ip_pack_table, ip_cts, pack_size, batch_size, bitlen);
//     pack_decryption_table(op_pack_table, op_cts, pack_size_1, batch_size_1, bitlen);
//     pack_decryption_table(opm_pack_table, op_mcts, pack_size_1, batch_size_1, bitlen);

//     //cout<<"First element (meth):"<<ip_pack_table[0]<<endl;
//     io->send_data(ip_pack_table, sizeof(uint64_t) * pack_size);
//     io->send_data(op_pack_table, sizeof(uint64_t) * pack_size_1);
//     io->send_data(opm_pack_table, sizeof(uint64_t) * pack_size_1);
//   } else {
//     io->recv_data(ip_pack_table, sizeof(uint64_t) * pack_size);
//     io->recv_data(op_pack_table, sizeof(uint64_t) * pack_size_1);
//     io->recv_data(opm_pack_table, sizeof(uint64_t) * pack_size_1);

//     unpack_decryption_table(ip_pack_table, ip_cts, pack_size, batch_size, bitlen);
//     unpack_decryption_table(op_pack_table, op_cts, pack_size_1, batch_size_1, bitlen);
//     unpack_decryption_table(opm_pack_table, op_mcts, pack_size_1, batch_size_1, bitlen);
//     //cout<<"First element (meth):"<<ip_pack_table[0]<<endl;

//     decrypt_ciphertexts(S, ip_cts, ip_ss, bitlen, nrelu, 1, true);
//     decrypt_ciphertexts(T, op_cts, op_ss, 1, nrelu, 0, false);
//     decrypt_ciphertexts(T, op_mcts, op_mss, 1, nrelu, 1, false);
//   }
// }

void parse_arguments(int argc, char**arg, int *party, int *port, int *bitlen, uint64_t *nrelu) {
  *party = atoi (arg[1]);
   address = arg[2];
	*port = atoi (arg[3]);

  if(argc >= 5) {
    *nrelu = atoi(arg[4]);
  }

  if(argc >= 6) {
    *bitlen = atoi(arg[5]);
  }
  

  if(argc >= 7) {
    verify = atoi(arg[6]);
  }

  if(argc >= 8) {
    num_threads = atoi(arg[7]);
  }
}

void thread_process(int tid, int party, uint64_t* inputs, uint64_t nrelu, int bitlen, uint64_t* ip_ss, uint64_t* op_ss, uint64_t* op_mss, uint64_t mac_key) {
  setup_semi_honest_mult(ioArr[tid], party, tid);

  uint64_t nr_per_thread = nrelu/num_threads;
  uint64_t r = nrelu % num_threads;
  uint64_t actual_per_thread;
  if(tid == num_threads-1)
      actual_per_thread = nr_per_thread + r;
  else
      actual_per_thread = nr_per_thread;
  uint64_t offset = tid*nr_per_thread;
  comparison(party, tid, ioArr[tid], inputs+offset, actual_per_thread, ip_ss+offset, op_ss+offset, op_mss+offset, mac_key, emp::delta_blocks, bitlen);

  ioArr[tid]->flush();
  finalize_semi_honest();
}

// void thread_process_1(int tid, int party, uint64_t* inputs, uint64_t nrelu, int bitlen, uint64_t* ip_ss, uint64_t* op_ss, uint64_t* op_mss) {
//   uint64_t *ptr = inputs+4608;
//   setup_semi_honest_mult(ioArr[tid], party, tid);
//   int prev_ctr=0;
//   int len = *(&CIFAR10_RELUS+1)-CIFAR10_RELUS;
//   for(int i=0; i<len; i++) {
//     uint64_t num_relu_layer = CIFAR10_RELUS[i];
//     uint64_t nr_per_thread = num_relu_layer/num_threads;
//     uint64_t r = num_relu_layer % num_threads;
//     uint64_t actual_per_thread;
//     if(tid ==num_threads-1)
//       actual_per_thread = nr_per_thread + r;
//     else
//       actual_per_thread = nr_per_thread;
//     uint64_t offset = prev_ctr + tid*nr_per_thread;
//     //cout<<"Thread id: "<<tid<<", Offset: "<<offset<<", NR Threads: "<<nr_per_thread<<"Actual Threads: "<<actual_per_thread<<endl;
//     //cout<<"Thread id:"<<tid<<", First Value (Out): "<<*(inputs+offset)<<endl;
//     comparison(party, tid, ioArr[tid], inputs+offset, actual_per_thread, bitlen, ip_ss+offset, op_ss+offset, op_mss+offset);
//     prev_ctr += CIFAR10_RELUS[i];
//   }
//   ioArr[tid]->flush();
//   finalize_semi_honest();
// }



int main(int argc, char** argv) {
  srand(time(NULL));
  int party, bitlen;
  uint64_t nrelu=def_nrelu;
  //Parse input arguments and configure parameters
	parse_arguments(argc, argv, &party, &port, &bitlen, &nrelu);
  // nrelu = 5;
  cout<<"Executing Comparison ..."<<endl;
  cout << "=====================Configuration======================" << endl;
  cout<<"Role: "<< party<<" - IP Address: "<< address <<" - Port: "<<port<<" - Benchmark: "<<benchmark<<" - Bitlength: "<<bitlen<<endl;
  cout << "========================================================" << endl;
  //Prepare Inputs
  std::random_device rd;
  std::mt19937_64 eng(rd());
  std::uniform_int_distribution<uint64_t> distr;

  uint64_t* inputs=(uint64_t *)malloc(nrelu*sizeof(uint64_t));
  memset(inputs, 0, nrelu*sizeof(uint64_t));
  if(party==ALICE){
    for(uint64_t i = 0; i < nrelu; ++i)
      inputs[i] = distr(eng)%prime_mod;
    // for(uint64_t i = 0; i < nrelu; ++i){
    //   if(i%2==0) inputs[i] = 1%prime_mod;
    //   else inputs[i] = (prime_mod - 1)%prime_mod;
    // }
  }

  uint64_t *ip_ss = (uint64_t *)malloc(nrelu*sizeof(uint64_t));
  uint64_t *op_ss = (uint64_t *)malloc(nrelu*sizeof(uint64_t));
  uint64_t *op_mss = (uint64_t *)malloc(nrelu*sizeof(uint64_t));

  for(int i=0; i <num_threads; i++) {
    ioArr[i] = new NetIO(party==ALICE ? nullptr : address.c_str(), port+i);
  }

  //Communication Initialization
  for(int i=0; i<num_threads; i++)
    start_comm[i] = ioArr[i]->counter;

  //Time Begin
  auto start = clock_start();

  if(party == ALICE) {
    prg.random_data(&mac_key, 8);
    mac_key = mac_key%prime_mod;
    // mac_key = 1;
  }

  std::thread relu_threads[num_threads];
  for(int i=0; i<num_threads; i++) {
      relu_threads[i] = std::thread(thread_process, i, party, inputs, nrelu, bitlen, ip_ss, op_ss, op_mss, mac_key);
  }

  //Join
  for(int i=0; i<num_threads; i++) {
    relu_threads[i].join();
  }
  //Time End
  long long t = time_from(start);
  std::cout << "######################Performance#######################" << std::endl;
  std::cout << "Time Taken: " << t <<" us" << std::endl;
  std::cout<<"Time Taken: "<<t/1000.0<<" ms"<<std::endl;
  std::cout<<"Time Taken: "<<t/1000000.0<<" s"<<std::endl;
  //Calculate Communication
  comm_sent = 0;
  for(int i=0; i<num_threads; i++) {
    comm_sent += (ioArr[i]->counter-start_comm[i]);
  }
  std::cout <<"Sent Data (B): " << comm_sent << std::endl;
  std::cout<<"Sent Data (KB): "<<(comm_sent>>10)<< std::endl;
  std::cout<<"Sent Data (MB): "<<(comm_sent>>20)<< std::endl;
  std::cout << "########################################################" <<  std::endl;

  //Test Protocol
  if(verify) {
    ioArr[0] = new NetIO(party==ALICE ? nullptr : address.c_str(), port);
    cout<<"ncomp: "<<nrelu<<endl;
    if(party == BOB) {
      ioArr[0]->send_data(inputs, sizeof(uint64_t) * nrelu);
      ioArr[0]->send_data(ip_ss, sizeof(uint64_t) * nrelu);
      ioArr[0]->send_data(op_ss, sizeof(uint64_t) * nrelu);
      ioArr[0]->send_data(op_mss, sizeof(uint64_t) * nrelu);
    } else {
      uint64_t inputs_1[nrelu];
      uint64_t inputs_res[nrelu];
      uint64_t relu_res[nrelu];

      uint64_t *ip_ssc = (uint64_t *)malloc(nrelu*sizeof(uint64_t));
      uint64_t *op_ssc = (uint64_t *)malloc(nrelu*sizeof(uint64_t));
      uint64_t *op_mssc = (uint64_t *)malloc(nrelu*sizeof(uint64_t));

      ioArr[0]->recv_data(inputs_1, sizeof(uint64_t) * nrelu);
      ioArr[0]->recv_data(ip_ssc, sizeof(uint64_t) * nrelu);
      ioArr[0]->recv_data(op_ssc, sizeof(uint64_t) * nrelu);
      ioArr[0]->recv_data(op_mssc, sizeof(uint64_t) * nrelu);
      for(uint64_t i=0; i< nrelu; i++) {
        inputs_res[i] = (inputs_1[i] + inputs[i])%prime_mod;
        if(inputs_res[i] < prime_mod/2) {
          relu_res[i] = 0;
        } else {
          relu_res[i] = 1;
        }
      }

      uint64_t ip_shares, ip_corr, op_shares, op_corr, opm_shares, opm_corr;
      uint64_t ctr_ip=0, ctr_op=0, ctr_opm=0;

      for(uint64_t i=0; i<nrelu; i++) {
        // std::cout << "ip_shares_0: "<< ip_ss[i] << std::endl;
        // std::cout << "ip_shares_1: "<< ip_ssc[i] << std::endl;
        ip_shares = (ip_ss[i]+ip_ssc[i])%prime_mod;
        ip_corr = mod_mult(mac_key, inputs_res[i], prime_mod);
        // std::cout << "mac_key: "<< mac_key << std::endl;
        // std::cout << "inputs_res[i]: "<< inputs_res[i] << std::endl;
        // std::cout << "ip_shares: "<< ip_shares << std::endl;
        // std::cout << "ip_corr: "<< ip_corr << std::endl;
        if(ip_shares == ip_corr)
          ctr_ip++;

        op_shares = (op_ss[i]+op_ssc[i])%prime_mod;
        // std::cout << "op_shares: "<< op_shares << std::endl;
        if(op_shares == relu_res[i])
            ctr_op++;

        opm_shares = (op_mss[i] + op_mssc[i])%prime_mod;
        // std::cout << "opm_shares: "<< opm_shares << std::endl;
        opm_corr = mod_mult(mac_key, relu_res[i], prime_mod);
        if(opm_shares == opm_corr)
          ctr_opm++;
      }
      cout << "**********************Verification**********************" <<endl;
      cout<<"Correct Input Macs: "<< ctr_ip <<endl;
      cout<<"Correct Outputs: "<< ctr_op <<endl;
      cout<<"Correct Output Macs: "<< ctr_opm <<endl;
      cout << "********************************************************" <<endl;
    }
  }
  //Performance Result

}
