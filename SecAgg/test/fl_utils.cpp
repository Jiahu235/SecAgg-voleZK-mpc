#include "fl_utils.h"

using namespace std;
using namespace emp;
using namespace seal;

// mult
uint64_t prime_mod = PLAINTEXT_MODULUS;
int32_t bitlength = 59;
int num_threads = 1;
uint64_t verify = 0;

// normball
block block_default = makeBlock(0ULL, 0ULL);
// cossim
uint64_t bound_cos = 1 << (SCALE-3);
uint64_t bound_ln = 1 << (SCALE+FIXINT);
uint64_t bound_l2 = 1 << (SCALE+8);
uint64_t allow_error = 1 << 6;

// overhead
uint64_t s_comm = 0;
uint64_t c_comm = 0;

uint64_t v_comm = 0;
uint64_t swap_comm = 0;
uint64_t l2_comm = 0;
uint64_t ln_comm = 0;
uint64_t mult_comm = 0;
uint64_t comp_comm = 0;

double v_time = 0;
double swap_time = 0;
double l2_time = 0;
double ln_time = 0;
double mult_time = 0;
double comp_time = 0;
double wait_time = 0;

// HE
FCField he_fc;


void Zk2Mpc(int party, NetIO *io, IntFp *x, uint64_t len_x, uint64_t *y, uint64_t len_y, uint64_t *x_value, uint64_t *mac_M, uint64_t *delta, uint64_t *mac_K){
  if(party==ALICE){
    for(uint64_t i=0; i<len_x; i++){
      x_value[i] = (uint64_t)HIGH64(x[i].value);
      mac_M[i] = (uint64_t)LOW64(x[i].value);
    }
    io->send_data(x_value, len_x*sizeof(uint64_t));
    io->send_data(mac_M, len_x*sizeof(uint64_t));
    io->recv_data(delta, sizeof(uint64_t));
    io->recv_data(mac_K, len_x*sizeof(uint64_t));
	io->recv_data(y, len_y*sizeof(uint64_t));
    memset(x_value, 0, len_x*sizeof(uint64_t));
    memset(mac_M, 0, len_x*sizeof(uint64_t));
  }else{
    *delta = (uint64_t)LOW64(((ZKFpExecVer<NetIO> *)(ZKFpExec::zk_exec))->ostriple->delta);
    for(uint64_t i=0; i<len_x; i++){
      mac_K[i] = (uint64_t)LOW64(x[i].value);
    }
    io->recv_data(x_value, len_x*sizeof(uint64_t));
    io->recv_data(mac_M, len_x*sizeof(uint64_t));
    io->send_data(delta, sizeof(uint64_t));
    io->send_data(mac_K, len_x*sizeof(uint64_t));
	io->send_data(y, len_y*sizeof(uint64_t));
    memset(delta, 0, sizeof(uint64_t));
    memset(mac_K, 0, len_x*sizeof(uint64_t));
	memset(y, 0, len_y*sizeof(uint64_t));
  }
  io->flush();
}

void Zk2Mpc(int party, NetIO *io, uint64_t *x, uint64_t *mac_x, uint64_t len_x, uint64_t *delta){
  if(party==ALICE){
    io->send_data(x, len_x*sizeof(uint64_t));
    io->send_data(mac_x, len_x*sizeof(uint64_t));
    io->recv_data(delta, sizeof(uint64_t));
	io->recv_data(x, len_x*sizeof(uint64_t));
    io->recv_data(mac_x, len_x*sizeof(uint64_t));
  }else{
    uint64_t *tmp_x = new uint64_t[len_x];
	uint64_t *tmp_mac_x = new uint64_t[len_x];
    io->recv_data(tmp_x, len_x*sizeof(uint64_t));
    io->recv_data(tmp_mac_x, len_x*sizeof(uint64_t));
    io->send_data(delta, sizeof(uint64_t));
    io->send_data(x, len_x*sizeof(uint64_t));
	io->send_data(mac_x, len_x*sizeof(uint64_t));
    *delta = 0;
    memcpy(x, tmp_x, len_x*sizeof(uint64_t));
	memcpy(mac_x, tmp_mac_x, len_x*sizeof(uint64_t));
	delete[] tmp_x;
	delete[] tmp_mac_x;
  }
  io->flush();
}

void vector_multiplication(int party, NetIO* io, IntFp *x, uint64_t len_x, 
							uint64_t *y, uint64_t len_y, uint64_t *ss_z, uint64_t *ss_mac_z)
{
	assert(len_x == len_y);
	uint64_t len =len_x;
	// client
  	uint64_t *input_x = new uint64_t[len];
  	memset(input_x, 0, len*sizeof(uint64_t));
  	uint64_t *mac_M = new uint64_t[len];
  	memset(mac_M, 0, len*sizeof(uint64_t));
  	// server
  	uint64_t mac_key = 0;
  	uint64_t *mac_K = new uint64_t[len];
  	memset(mac_K, 0, len*sizeof(uint64_t));
	uint64_t *tmp_y = new uint64_t[len];
  	memcpy(tmp_y, y, len*sizeof(uint64_t));

  	uint64_t start_comm = io->counter;
	auto start_time = clock_start();
  	Zk2Mpc(party, io, x, len, tmp_y, len, input_x, mac_M, &mac_key, mac_K);  
	io->flush();
	v_time += time_from(start_time);
  	v_comm += (io->counter - start_comm);
  
	// FCField he_fc(party, io);
  	uint64_t num_cols = 1;
  	uint64_t num_rows = 1;

  	//Setup Input objects
  	vector<vector<uint64_t>> matrix;
  	matrix.resize(num_rows, vector<uint64_t>(len, 0));
  	vector<vector<uint64_t>> input_share;
  	input_share.resize(len, vector<uint64_t>(num_cols, 0));
  	vector<vector<uint64_t>> mac_M_vec;
  	mac_M_vec.resize(len, vector<uint64_t>(num_cols, 0));
  	vector<vector<uint64_t>> mac_K_vec;
  	mac_K_vec.resize(len, vector<uint64_t>(num_cols, 0));

  	if(party == BOB){
    	for(uint64_t i=0; i<len; i++) {
    	  for(uint64_t j=0; j<num_cols; j++){
    	    input_share[i][j] = input_x[i];
    	    mac_M_vec[i][j] = mac_M[i];
    	    // std::cout << "input_share[" << i << "][" << j << "]:" << input_share[i][j] << std::endl;
    	  }
    	}
  	}else{
    	for(uint64_t i=0; i<len; i++) {
    		for(uint64_t j=0; j<num_cols; j++){
     	   	mac_K_vec[i][j] = mac_K[i];
    	  	}
    	}
    	//Create input matrix
    	for(uint64_t i=0; i< num_rows; i++) {
   			for(uint64_t j=0; j<len; j++){
      		  matrix[i][j] = tmp_y[j];
      		}
    	}
  	}

	std::cout << "---------- Vector multiplication running ------------" << std::endl;

	start_comm = io->counter;
	start_time = clock_start();
	he_fc.vector_multiplication_large_no_share_1(num_rows, len, num_cols, matrix, input_share, mac_M_vec, mac_K_vec, mac_key, 
                                              ss_z, ss_mac_z, seal::Modulus(prime_mod), verify, false, wait_time);
	io->flush();
	double mult_cost_time = time_from(start_time);
	uint64_t mult_cost_comm = (io->counter - start_comm);
	swap_time += mult_cost_time;
	swap_comm += mult_cost_comm;
	mult_time += mult_cost_time;
	mult_comm += mult_cost_comm;
	std::cout << "---------- Vector multiplication finish ------------" << std::endl;
	delete[] input_x;
	delete[] mac_M;
	delete[] mac_K;
	delete[] tmp_y;
}

void vector_bool_multiplication(int party, NetIO* io, IntFp *x, 
								uint64_t len_x, uint64_t y, uint64_t *ss_z, uint64_t *ss_mac_z)
{
	assert(y*(y-1)==0);
	uint64_t len =len_x;
	// client
  	uint64_t *input_x = new uint64_t[len];
  	memset(input_x, 0, len*sizeof(uint64_t));
  	uint64_t *mac_M = new uint64_t[len];
  	memset(mac_M, 0, len*sizeof(uint64_t));
  	// server
  	uint64_t mac_key = 0;
  	uint64_t *mac_K = new uint64_t[len];
  	memset(mac_K, 0, len*sizeof(uint64_t));

  	uint64_t start_comm = io->counter;
	auto start_time = clock_start();
  	Zk2Mpc(party, io, x, len, &y, 1, input_x, mac_M, &mac_key, mac_K);  
  	io->flush();
	v_time += time_from(start_time);
	v_comm += (io->counter - start_comm);
  
	// FCField he_fc(party, io);
  	uint64_t num_cols = 1;
  	uint64_t num_rows = 1;

  	//Setup Input objects
  	vector<vector<uint64_t>> input_share;
  	input_share.resize(len, vector<uint64_t>(num_cols, 0));
  	vector<vector<uint64_t>> mac_M_vec;
  	mac_M_vec.resize(len, vector<uint64_t>(num_cols, 0));
  	vector<vector<uint64_t>> mac_K_vec;
  	mac_K_vec.resize(len, vector<uint64_t>(num_cols, 0));

  	if(party == BOB){
    	for(uint64_t i=0; i<len; i++) {
    	  for(uint64_t j=0; j<num_cols; j++){
    	    input_share[i][j] = input_x[i];
    	    mac_M_vec[i][j] = mac_M[i];
    	    // std::cout << "input_share[" << i << "][" << j << "]:" << input_share[i][j] << std::endl;
    	  }
    	}
  	}else{
    	for(uint64_t i=0; i<len; i++) {
    		for(uint64_t j=0; j<num_cols; j++){
     	   	mac_K_vec[i][j] = mac_K[i];
    	  	}
    	}
  	}

	start_comm = io->counter;
	start_time = clock_start();
	he_fc.vector_bool_multiplication_large_no_share(num_rows, len, num_cols, y, input_share, mac_M_vec, mac_K_vec, mac_key, 
                                              	ss_z, ss_mac_z, seal::Modulus(prime_mod), verify, false, wait_time);
	io->flush();
	double mult_cost_time = time_from(start_time);
	uint64_t mult_cost_comm = (io->counter - start_comm);
	swap_time += mult_cost_time;
	swap_comm += mult_cost_comm;
	mult_time += mult_cost_time;
	mult_comm += mult_cost_comm;
	delete[] input_x;
	delete[] mac_M;
	delete[] mac_K;
}

uint64_t mod_shift(uint64_t a, uint64_t b, uint64_t prime_mod) {
    __m128i temp=makeBlock(0, 0), stemp=makeBlock(0, 0);
    memcpy(&temp, &a, 8);
    stemp = SHL128(temp, b);

    uint64_t input[2];
    input[0] = stemp[0];
    input[1] = stemp[1];

    uint64_t result = seal::util::barrett_reduce_128(input, seal::Modulus(prime_mod));

    return result;
}


//Referred SCI OT repo's logic to pack ot messages
void pack_decryption_table(uint64_t *pack_table, uint64_t *ciphertexts, int pack_size, int batch_size, int bitlen) {
  uint64_t beg_idx = 0;
  uint64_t end_idx = 0;
  uint64_t beg_blk = 0;
  uint64_t end_blk = 0;
  uint64_t temp_blk = 0;
  uint64_t mask = (1ULL << bitlen) - 1;
  uint64_t pack_blk_size = 64;

  if (bitlen == 64)
    mask = -1;

  for (int i = 0; i < pack_size; i++) {
    pack_table[i] = 0;
  }

  for (int i = 0; i < batch_size; i++) {
    beg_idx = i * bitlen;
    end_idx = beg_idx + bitlen;
    end_idx -= 1;
    beg_blk = beg_idx / pack_blk_size;
    end_blk = end_idx / pack_blk_size;

    if (beg_blk == end_blk) {
      pack_table[beg_blk] ^= (ciphertexts[i] & mask) << (beg_idx % pack_blk_size);
    } else {
      temp_blk = (ciphertexts[i] & mask);
      pack_table[beg_blk] ^= (temp_blk) << (beg_idx % pack_blk_size);
      pack_table[end_blk] ^= (temp_blk) >> (pack_blk_size - (beg_idx % pack_blk_size));
    }
  }
}

//Referred SCI OT repo's logic to unpack ot messages
void unpack_decryption_table(uint64_t *pack_table, uint64_t *ciphertexts, int pack_size, int batch_size, int bitlen) {
  uint64_t beg_idx = 0;
  uint64_t end_idx = 0;
  uint64_t beg_blk = 0;
  uint64_t end_blk = 0;
  uint64_t temp_blk = 0;
  uint64_t mask = (1ULL << bitlen) - 1;
  uint64_t pack_blk_size = 64;

  for (int i = 0; i < batch_size; i++) {
    beg_idx = i * bitlen;
    end_idx = beg_idx + bitlen - 1;
    beg_blk = beg_idx / pack_blk_size;
    end_blk = end_idx / pack_blk_size;

    if (beg_blk == end_blk) {
      ciphertexts[i] = (pack_table[beg_blk] >> (beg_idx % pack_blk_size)) & mask;
    } else {
      ciphertexts[i] = 0;
      ciphertexts[i] ^= (pack_table[beg_blk] >> (beg_idx % pack_blk_size));
      ciphertexts[i] ^= (pack_table[end_blk] << (pack_blk_size - (beg_idx % pack_blk_size)));
      ciphertexts[i] = ciphertexts[i] & mask;
    }
  }
}

void create_ciphertexts(Integer *garbled_data, block label_delta, uint64_t *ciphertexts, uint64_t* server_shares, int bitlen, uint64_t nrelu, uint64_t alpha, int l_idx, bool apply_prg) {
  uint64_t delta_int;
  memcpy(&delta_int, &label_delta[l_idx], 8);

  uint64_t mask = (1ULL << bitlen) - 1;
  block seed, label_block_0, label_block_1;
  uint64_t label_temp_0, label_temp_1;
  uint64_t **random_val = (uint64_t **)malloc(nrelu*sizeof(uint64_t*));
  uint8_t pnp, cpnp;
  for(uint64_t i=0; i<nrelu; i++) {
    random_val[i] = (uint64_t *)malloc(bitlen*sizeof(uint64_t));
  }

  for(uint64_t i=0; i<nrelu; i++) {
    server_shares[i] = 0;
    for(int j=0; j<bitlen; j++) {
      if(apply_prg) {
        memcpy(&seed, &garbled_data[i].bits[j].bit, 16);
        PRG prg0(&seed);
        prg0.random_data(&label_block_0, 16);
        seed = garbled_data[i].bits[j].bit^label_delta;
        PRG prg1(&seed);
        prg1.random_data(&label_block_1, 16);

        memcpy(&label_temp_0, &label_block_0[l_idx], 8);
        memcpy(&label_temp_1, &label_block_1[l_idx], 8);

      } else {
        memcpy(&label_temp_0, &garbled_data[i].bits[j].bit[l_idx], 8);
        label_temp_1 = label_temp_0^delta_int;
      }

      PRG prg;
      prg.random_data(&random_val[i][j], 8);
      random_val[i][j] %= prime_mod;
      // random_val[i][j] = 0%prime_mod;
      pnp = (garbled_data[i].bits[j].bit[0]) & 1;
      cpnp = 1 - pnp;
      ciphertexts[(i*bitlen+j)*2+pnp] = (random_val[i][j])^(label_temp_0 & mask);
      ciphertexts[(i*bitlen+j)*2+cpnp] = ((random_val[i][j]+alpha)%prime_mod)^(label_temp_1 & mask);
      server_shares[i] = (server_shares[i] + mod_shift(random_val[i][j],j,prime_mod))%prime_mod;
    }
    server_shares[i] = (prime_mod - server_shares[i])%prime_mod;
  }
  for(uint64_t i=0; i<nrelu; i++) {
    free(random_val[i]);
  }
  free(random_val);
}

void decrypt_ciphertexts(Integer *garbled_data, uint64_t *ciphertexts, uint64_t* client_shares, int bitlen, uint64_t nrelu, int l_idx, bool apply_prg) {
  uint64_t label_temp;
  block label_block;
  uint8_t pnp;
  uint64_t random_val;

  uint64_t mask = (1ULL << bitlen) - 1;

  for(uint64_t i=0; i<nrelu; i++) {
    client_shares[i] = 0;
    for(int j=0; j< bitlen; j++) {
      if(apply_prg) {
        PRG prg(&garbled_data[i].bits[j].bit);
        prg.random_data(&label_block, 16);

        memcpy(&label_temp, &label_block[l_idx], 8);
      } else {
        memcpy(&label_temp, &garbled_data[i].bits[j].bit[l_idx], 8);
      }

      pnp = (garbled_data[i].bits[j].bit[0]) & 1;
      random_val = ciphertexts[(i*bitlen+j)*2+pnp]^(label_temp & mask);
      // std::cout << "random_val[" << j << "]:" << random_val << std::endl;
      client_shares[i] = (client_shares[i] + mod_shift(random_val,j,prime_mod))%prime_mod;
    }
  }
}

void comparison(int party, int tid, NetIO* io, uint64_t* inputs, uint64_t nrelu, 
				uint64_t* ip_ss, uint64_t* op_ss, uint64_t* op_mss, uint64_t mac_key, block *delta_blocks, int bitlen)
{
  bitlen = bitlen + 1;
  //Public prime values
  Integer p(bitlen, prime_mod, PUBLIC);
  Integer p_mod2(bitlen, prime_mod/2, PUBLIC);
  Integer zero(bitlen, 0, PUBLIC);

  //Assign Inputs
  Integer *X = new Integer[nrelu];
  for(uint64_t i = 0; i < nrelu; ++i)
    X[i] = Integer(bitlen, inputs[i], ALICE);
  Integer *Y = new Integer[nrelu];
  for(uint64_t i = 0; i < nrelu; ++i)
    Y[i] = Integer(bitlen, inputs[i], BOB);

  Integer *S = new Integer[nrelu];
  Integer *T = new Integer[nrelu];

  for(uint64_t i=0; i < nrelu; ++i) {
    //Perform mod p
    Integer s0 = X[i];
    //s0.resize(s0.size()+1);

    Integer s1 = Y[i];
   //s1.resize(s1.size()+1);

    Integer sum = s0 + s1;

    Integer mod_p_val = sum - p;

    Bit borrow_bit = mod_p_val[mod_p_val.size()-1];

    Integer s = mod_p_val.select(borrow_bit, sum);

    S[i] = s;

    //Perform Comparison
    Integer p2_minus_s = p_mod2 - s;

    Bit is_negative = p2_minus_s[p2_minus_s.size()-1];
    vector<Bit> bit_res;
    bit_res.push_back(is_negative);
    Integer res = Integer(bit_res);

    // Integer res = Integer(&is_negative);  //the result of comparison is 1 bit

    T[i] = res;
  }

  int pack_size = ceil(nrelu*bitlen*bitlen*2.0/(8*sizeof(uint64_t)));
  int pack_size_1 = ceil(nrelu*1*bitlen*2.0/(8*sizeof(uint64_t)));  //the result of comparison is 1 bit
  int batch_size = nrelu*bitlen*2;
  int batch_size_1 = nrelu*1*2;  //the result of comparison is 1 bit

  uint64_t *ip_cts = (uint64_t *)malloc(nrelu*bitlen*2*sizeof(uint64_t));

  uint64_t *op_cts = (uint64_t *)malloc(nrelu*1*2*sizeof(uint64_t));

  uint64_t *op_mcts = (uint64_t *)malloc(nrelu*1*2*sizeof(uint64_t));

  uint64_t *ip_pack_table = (uint64_t *)malloc(pack_size*sizeof(uint64_t));
  uint64_t *op_pack_table = (uint64_t *)malloc(pack_size_1*sizeof(uint64_t));
  uint64_t *opm_pack_table = (uint64_t *)malloc(pack_size_1*sizeof(uint64_t));

  if(party == ALICE) {

    create_ciphertexts(S, delta_blocks[tid], ip_cts, ip_ss, bitlen, nrelu, mac_key, 1, true);
    create_ciphertexts(T, delta_blocks[tid], op_cts, op_ss, 1, nrelu, 1, 0, false);
    create_ciphertexts(T, delta_blocks[tid], op_mcts, op_mss, 1, nrelu, mac_key, 1, false);

    pack_decryption_table(ip_pack_table, ip_cts, pack_size, batch_size, bitlen);
    pack_decryption_table(op_pack_table, op_cts, pack_size_1, batch_size_1, bitlen);
    pack_decryption_table(opm_pack_table, op_mcts, pack_size_1, batch_size_1, bitlen);

    //cout<<"First element (meth):"<<ip_pack_table[0]<<endl;
    io->send_data(ip_pack_table, sizeof(uint64_t) * pack_size);
    io->send_data(op_pack_table, sizeof(uint64_t) * pack_size_1);
    io->send_data(opm_pack_table, sizeof(uint64_t) * pack_size_1);
  } else {
    io->recv_data(ip_pack_table, sizeof(uint64_t) * pack_size);
    io->recv_data(op_pack_table, sizeof(uint64_t) * pack_size_1);
    io->recv_data(opm_pack_table, sizeof(uint64_t) * pack_size_1);

    unpack_decryption_table(ip_pack_table, ip_cts, pack_size, batch_size, bitlen);
    unpack_decryption_table(op_pack_table, op_cts, pack_size_1, batch_size_1, bitlen);
    unpack_decryption_table(opm_pack_table, op_mcts, pack_size_1, batch_size_1, bitlen);
    //cout<<"First element (meth):"<<ip_pack_table[0]<<endl;

    decrypt_ciphertexts(S, ip_cts, ip_ss, bitlen, nrelu, 1, true);
    decrypt_ciphertexts(T, op_cts, op_ss, 1, nrelu, 0, false);
    decrypt_ciphertexts(T, op_mcts, op_mss, 1, nrelu, 1, false);
  }
  io->flush();
  delete[] X;
  delete[] Y;
  delete[] S;
  delete[] T;
  free(ip_cts);
  free(op_cts);
  free(op_mcts);
  free(ip_pack_table);
  free(op_pack_table);
  free(opm_pack_table);
}

void Wrap_comparison(int party, int tid, NetIO* io, uint64_t *ss_x, uint64_t *ss_mac_x, uint64_t len_x,
					uint64_t *ss_z, uint64_t *ss_mac_z, uint64_t mac_delta, block *delta_blocks, bool ss_zero){
	uint64_t *ip_ss = new uint64_t[len_x];
  	// uint64_t *op_ss = new uint64_t[len_x];
 	// uint64_t *op_mss = new uint64_t[len_x];

	CircuitExecution *tmp_circ_exec = CircuitExecution::circ_exec;
	ProtocolExecution *tmp_prot_exec = ProtocolExecution::prot_exec;

	uint64_t start_comm = io->counter;
	auto start_time = clock_start();
	if(party == ALICE) {
		HalfGateGen<NetIO> * t = new HalfGateGen<NetIO>(io);
		CircuitExecution::circ_exec = t;
		ProtocolExecution::prot_exec = new SemiHonestGen<NetIO>(io, t);
		delta_blocks[tid] = t->delta;
	} else {
		HalfGateEva<NetIO> * t = new HalfGateEva<NetIO>(io);
		CircuitExecution::circ_exec = t;
		ProtocolExecution::prot_exec = new SemiHonestEva<NetIO>(io, t);
	}
	io->flush();
	v_time += time_from(start_time);
	v_comm += (io->counter - start_comm);

	std::cout << "---------- Comparison running ------------" << std::endl;

  	start_comm = io->counter;
	start_time = clock_start();
	comparison(party, tid, io, ss_x, len_x, ip_ss, ss_z, ss_mac_z, mac_delta, delta_blocks);
	double comp_cost_time = time_from(start_time);
	uint64_t comp_cost_comm = (io->counter - start_comm);
	io->flush();
	swap_time += comp_cost_time;
	swap_comm += comp_cost_comm;
	comp_time += comp_cost_time;
	comp_comm += comp_cost_comm;

	std::cout << "---------- Comparison finish ------------" << std::endl;

  	// io->flush();

	if(verify){
		if(party==BOB){
			io->send_data(ss_mac_x, len_x*sizeof(uint64_t));
			io->send_data(ip_ss, len_x*sizeof(uint64_t));
		}
		else{
			uint64_t *from_c_ss_mac = new uint64_t[len_x];
			uint64_t *from_c_ip_ss = new uint64_t[len_x];
			io->recv_data(from_c_ss_mac, len_x*sizeof(uint64_t));
			io->recv_data(from_c_ip_ss, len_x*sizeof(uint64_t));

			uint64_t *mac_input = new uint64_t[len_x];
			uint64_t *mac_output = new uint64_t[len_x];
			uint64_t ctr = 0;
			for(uint64_t i=0; i<len_x; i++){
				mac_input[i] = (ss_mac_x[i] + from_c_ss_mac[i])%prime_mod;
				mac_output[i] = (ip_ss[i] + from_c_ip_ss[i])%prime_mod;
				if(mac_input[i] == mac_output[i]) ctr++;
			}
			if(ctr < len_x) error("Wrap comparision failed!");
			delete[] from_c_ss_mac;
			delete[] from_c_ip_ss;
			delete[] mac_input;
			delete[] mac_output;
		}
	}
	io->flush();

	CircuitExecution *new_circ_exec = CircuitExecution::circ_exec;
	ProtocolExecution *new_prot_exec = ProtocolExecution::prot_exec;
  	CircuitExecution::circ_exec = tmp_circ_exec;
	ProtocolExecution::prot_exec = tmp_prot_exec;
	delete new_prot_exec;
	delete new_circ_exec;
	delete[] ip_ss;
}

uint64_t comm(BoolIO<NetIO> **ios, int num_threads)
{
	uint64_t c = 0;
	for (int i = 0; i < num_threads; ++i)
		c += ios[i]->io->counter;
	return c;
}

void L2Check(int party, IntFp *x, uint64_t len_x, uint64_t up_bd, uint64_t low_bd)
{
	std::cout << "---------- L2Check running ------------" << std::endl;
	// compute inner product x^2
	uint64_t sum = 0;
	if(party == ALICE){
		for(uint64_t i=0; i<len_x; i++){
			uint64_t *v = (uint64_t*)&(x[i].value);
			sum = add_mod(sum, mult_mod(v[1], v[1]));   // differece between mod_mult and mult_mod
		}
		// std::cout << "L2(sum_sqr): "<< sum 
		//  << "\nsqure_low_bound: "<< low_bd*low_bd 
		//  << "\nsqure_up_bound: "<< up_bd*up_bd << std::endl;
	}

	// commit x^2
	IntFp sqr_x = IntFp(sum, ALICE);
	// std::cout << "Commit finished" << std::endl;

	// check x^2 - new way proposed in QuickSilver
	IntFp *poly1 = new IntFp[len_x + 1];
	IntFp *poly2 = new IntFp[len_x + 1];
	for (uint64_t i = 0; i < len_x; i++){
		poly1[i] = x[i];
		poly2[i] = x[i];
	}
	poly1[len_x] = sqr_x;
	poly2[len_x] = IntFp(-1, PUBLIC);
	fp_zkp_inner_prdt<BoolIO<NetIO>>(poly1, poly2, 0, len_x+1);

	// old way
	// IntFp check_sqr = x[0] * x[0];
	// for(uint64_t i=1; i<len_x; i++){
	// 	check_sqr = check_sqr + x[i] * x[i];
	// }
	// check_sqr = check_sqr + sqr_x.negate();
	// // std::cout << "check_sqr:" << ((uint64_t*)&(check_sqr.value))[1] << std::endl;
	// // check_sqr.reveal_zero();
	// uint64_t z = 0;
	// batch_reveal_check(&check_sqr, &z, 1);

	// truncate by s bits
	IntFp *trunc = new IntFp[1];
	trunc[0] = sqr_x;
	IntFp *trunc_x2 = new IntFp[1];
	ZKpositiveTruncAny(party, trunc, trunc_x2, 1, SCALE);
	uint64_t new_bd0 = (low_bd * low_bd) >> SCALE;
	uint64_t new_bd1 = (up_bd  * up_bd)  >> SCALE;
	std::cout << "Truncation finished! " << endl;

	// check x^2/2^s truncation in range [low_bd^2/2^s, up_bd^2/2^s]
	// old LUT approach (for reference):
	// // IntFp tmp = trunc_x2[0] + IntFp(new_bd0).negate();
	// IntFp tmp = IntFp(new_bd0+1) + IntFp(new_bd0).negate();
	// LUTRangeIntFp *LUTRange2 = new LUTRangeIntFp(party);
	// uint64_t t = (new_bd1 - new_bd0)%PR;
	// // uint64_t t = ((up_bd*up_bd - low_bd*low_bd) / (uint64_t)(pow(2,SCALE)))%PR;
	// LUTRange2->LUTRangeinit(t);
	// LUTRange2->LUTRangeread(tmp);
	// delete LUTRange2;

	// --- OR-proof range check: trunc_x2[0] ∈ [new_bd0, new_bd1] ---
	// Replaces LUTRangeIntFp with the 1-hotvector OR proof from Section 4.1
	// of "Post-Quantum Threshold Ring Signatures from VOLE-in-the-Head" (2025).
	//
	// Claim: ∃ t_i ∈ {new_bd0,...,new_bd1} s.t. [trunc_x2] - t_i = 0.
	// The active index idx = trunc_x2 - new_bd0 is encoded as idx = ii1*sqrt_n + ii2
	// using two 1-hotvectors b1[0..sqrt_n-1] and b2[0..sqrt_n-1].
	// The range is padded to sqrt_n^2 entries (accepts [new_bd0, new_bd0+sqrt_n^2-1],
	// a small relaxation of at most 2*sqrt_n extra values).
	//
	// Three QuickSilver constraints:
	//   A) Σ_k b1[k] = 1, Σ_k b2[k] = 1                          (degree 1)
	//   B) (σ_j - k) * b_{j,k} = 0  ∀k,j   (single active bit)   (degree 2)
	//   C) trunc_x2 - new_bd0 - σ1*sqrt_n - σ2 = 0               (degree 1, LINEAR)
	// VOLE witness: 2*sqrt_n bits vs. O(n) for the LUT permutation.
	{
		uint64_t n     = new_bd1 - new_bd0 + 1;
		uint64_t sqrt_n = (uint64_t)ceil(sqrt((double)n));
		while(sqrt_n * sqrt_n < n) sqrt_n++;  // ensure sqrt_n^2 >= n

		// Prover computes the active decomposition (ii1, ii2) s.t. idx = ii1*sqrt_n + ii2
		uint64_t tx2_clear = (party == ALICE) ? (uint64_t)HIGH64(trunc_x2[0].value) : 0;
		uint64_t active    = (party == ALICE) ? (tx2_clear - new_bd0) : 0;
		uint64_t ii1 = active / sqrt_n;
		uint64_t ii2 = active % sqrt_n;

		// Constraint A: Σ_k b_{j,k} = 1  (each hotvector sums to 1)
		// Commit only the first sqrt_n-1 bits of each hotvector.
		// The last bit is derived: b[sqrt_n-1] = 1 - Σ_{k=0}^{sqrt_n-2} b[k]
		// This is a free linear combination — zero extra VOLEs, no inner product check.
		// Sum = 1 is enforced structurally by construction (paper Section 4.1.3).
		IntFp *b1 = new IntFp[sqrt_n];
		IntFp *b2 = new IntFp[sqrt_n];
		IntFp sum_b1 = IntFp(0ULL, PUBLIC);
		IntFp sum_b2 = IntFp(0ULL, PUBLIC);
		for(uint64_t k = 0; k < sqrt_n - 1; k++){
			b1[k] = IntFp((party == ALICE && k == ii1) ? 1ULL : 0ULL, ALICE);
			b2[k] = IntFp((party == ALICE && k == ii2) ? 1ULL : 0ULL, ALICE);
			sum_b1 = sum_b1 + b1[k];
			sum_b2 = sum_b2 + b2[k];
		}
		b1[sqrt_n-1] = IntFp(1ULL, PUBLIC) + sum_b1.negate();  // 1 - Σ prior b1[k]
		b2[sqrt_n-1] = IntFp(1ULL, PUBLIC) + sum_b2.negate();  // 1 - Σ prior b2[k]

		// σ_j = Σ_k b_{j,k} * k  (CMult by public constant k: linear, degree 1)
		IntFp sigma1 = IntFp(0ULL, PUBLIC);
		IntFp sigma2 = IntFp(0ULL, PUBLIC);
		for(uint64_t k = 1; k < sqrt_n; k++){
			sigma1 = sigma1 + b1[k] * IntFp(k, PUBLIC);
			sigma2 = sigma2 + b2[k] * IntFp(k, PUBLIC);
		}

		// Constraint B: (σ_j - k) * b_{j,k} = 0  ∀k  (exactly one bit is 1)
		// Batched via fp_zkp_inner_prdt with random chi (soundness 1/PR per check).
		// Proof: if two bits b_{j,a}, b_{j,c} were both non-zero, σ_j could not equal
		// the position of either, so the batch check catches any malformed hotvector.
		{
			IntFp *A1 = new IntFp[sqrt_n];
			IntFp *A2 = new IntFp[sqrt_n];
			for(uint64_t k = 0; k < sqrt_n; k++){
				uint64_t neg_k = (k == 0) ? 0ULL : (PR - k);
				A1[k] = sigma1 + IntFp(neg_k, PUBLIC);  // σ1 - k
				A2[k] = sigma2 + IntFp(neg_k, PUBLIC);  // σ2 - k
			}
			fp_zkp_inner_prdt<BoolIO<NetIO>>(A1, b1, 0, sqrt_n);
			fp_zkp_inner_prdt<BoolIO<NetIO>>(A2, b2, 0, sqrt_n);
			delete[] A1;
			delete[] A2;
		}

		// Constraint C: trunc_x2[0] = new_bd0 + σ1*sqrt_n + σ2  (linear!)
		// σ1*sqrt_n is CMult (public scalar): still degree 1.
		// This is the OR proof's core: it certifies [trunc_x2] - t_{active} = 0.
		{
			uint64_t neg_bd0 = (new_bd0 == 0) ? 0ULL : (PR - new_bd0);
			IntFp lhs = trunc_x2[0]
			          + IntFp(neg_bd0, PUBLIC)                        // - new_bd0
			          + (sigma1 * IntFp(sqrt_n, PUBLIC)).negate()     // - σ1*sqrt_n
			          + sigma2.negate();                               // - σ2
			IntFp one_pub = IntFp(1ULL, PUBLIC);
			fp_zkp_inner_prdt<BoolIO<NetIO>>(&lhs, &one_pub, 0, 1);
		}

		delete[] b1;
		delete[] b2;
	}
	std::cout << "All L2 checks finished (OR proof)! " << endl;
	delete[] poly1;
	delete[] poly2;
	delete[] trunc;
	delete[] trunc_x2;

	// check x^2 in range [low_bd^2, up_bd^2]
	// // IntFp tmp = sqr_x + IntFp(low_bd*low_bd).negate();
	// IntFp tmp = IntFp(low_bd*low_bd+1) + IntFp(low_bd*low_bd).negate();
	// LUTRangeIntFp *LUTRange = new LUTRangeIntFp(party);
	// uint64_t t = (up_bd*up_bd - low_bd*low_bd)%PR;
	// LUTRange->LUTRangeinit(t);
	// LUTRange->LUTRangeread(tmp);
	// delete LUTRange;
	// std::cout << "All L2 checks finished! " << endl;
}

void LnCheck(int party, IntFp *x, uint64_t len_x, uint64_t bd)
{
	std::cout << "---------- LnCheck running ------------" << std::endl;
	// parse hx = |x|
	uint64_t *hx = new uint64_t[len_x];
	if(party == ALICE){
		for(uint64_t i=0; i<len_x; i++){
			uint64_t *v = (uint64_t*)&(x[i].value);
			hx[i] = (v[1] <= (PR-1)/2) ? v[1] : PR - v[1];
		}
	}

	// commit s and hx, obtain sign and ab_x
	// new way: commit ab_x, check (ab_x - x)(ab_x + x) =0
	// IntFp *sign = new IntFp[len_x];
	IntFp *ab_x = new IntFp[len_x];
	for(uint64_t i=0; i<len_x; i++){
		// sign[i] = IntFp(s[i], ALICE);
		ab_x[i] = IntFp(hx[i], ALICE);
	}
	// std::cout << "Commit finished" << std::endl;

	// // check x = sign *ab_x and sign*(sign-1)=0 - new way proposed in QuickSilver
	// check ab_x^2 - x^2 =0
	IntFp *poly1 = new IntFp[len_x];
	IntFp *poly2 = new IntFp[len_x];
	for (uint64_t i = 0; i < len_x; i++){
		poly1[i] = ab_x[i] + x[i].negate();
		poly2[i] = ab_x[i] + x[i];
	}
	fp_zkp_inner_prdt<BoolIO<NetIO>>(poly1, poly2, 0, len_x);

	// check ab_x in range [0, bd]
	LUTRangeIntFp *LUTRange1 = new LUTRangeIntFp(party);
	LUTRange1->LUTRangeinit(bd);
	for(int i=0; i<len_x; i++){
	  	LUTRange1->LUTRangeread(ab_x[i]);
	}
	delete LUTRange1;
	std::cout << "All Ln checks finished! " << endl;
	delete[] hx;
	delete[] ab_x;
	delete[] poly1;
	delete[] poly2;
}

// z = vec(x) cdot vec(y), x and mac_x are shared by server and client, y is held by server
void Ideal_vector_multiplication(int party, NetIO* io, uint64_t *ss_x, uint64_t *ss_mac_x, uint64_t len_x, 
									uint64_t *y, uint64_t len_y, 
									uint64_t *ss_z, uint64_t *ss_mac_z, uint64_t mac_delta, bool ss_zero)
{
	assert(len_x == len_y);
	// std::cout << "---------- Ideal_vector_multiplication running ------------" << std::endl;
	uint64_t len_z = 1;  // length of z is 1
	if(party == ALICE){
		io->send_data(ss_x, len_x*sizeof(uint64_t));
		io->send_data(ss_mac_x, len_x*sizeof(uint64_t));
		if(ss_zero){
			std::memset(ss_z, 0, len_z*sizeof(uint64_t));
			std::memset(ss_mac_z, 0, len_z*sizeof(uint64_t));
		}else{
			io->recv_data(ss_z, len_z*sizeof(uint64_t));
			io->recv_data(ss_mac_z, len_z*sizeof(uint64_t));
		}
	}else{
		uint64_t *from_c_ss_x = new uint64_t[len_x];
		uint64_t *from_c_ss_mac_x = new uint64_t[len_x];
		io->recv_data(from_c_ss_x, len_x*sizeof(uint64_t));
		io->recv_data(from_c_ss_mac_x, len_x*sizeof(uint64_t));

		uint64_t *real_x = new uint64_t[len_x];
		uint64_t *real_mac_x = new uint64_t[len_x];
		for(uint64_t i=0; i<len_x; i++){
			real_x[i] = (from_c_ss_x[i] + ss_x[i])%prime_mod;
			real_mac_x[i] = (from_c_ss_mac_x[i] + ss_mac_x[i])%prime_mod;
			if(mult_mod(real_x[i], mac_delta) != real_mac_x[i]){
				error("Mac mult input check failed!\n");
			}
			*ss_z = (*ss_z + mult_mod(real_x[i], y[i]))%prime_mod;
			*ss_mac_z = (*ss_mac_z + mult_mod(real_mac_x[i], y[i]))%prime_mod;
			if(mult_mod(*ss_z, mac_delta) != *ss_mac_z){
				error("Mac mult output check failed!\n");
			}
		}
		if(!ss_zero){
			PRG prg;
			uint64_t to_c_ss_z;
			uint64_t to_c_ss_mac_z;
			random_mod_p(prg, &to_c_ss_z, len_z, prime_mod);
			random_mod_p(prg, &to_c_ss_mac_z, len_z, prime_mod);
			io->send_data(&to_c_ss_z, len_z*sizeof(uint64_t));
			io->send_data(&to_c_ss_mac_z, len_z*sizeof(uint64_t));
			*ss_z = (*ss_z + prime_mod - to_c_ss_z)%prime_mod;
			*ss_mac_z = (*ss_mac_z + prime_mod - to_c_ss_mac_z)%prime_mod;
		}
		delete[] from_c_ss_x;
		delete[] from_c_ss_mac_x;
		delete[] real_x;
		delete[] real_mac_x;
	}
	// std::cout << "---------- Ideal_vector_multiplication finish ------------" << std::endl;
	io->flush();
}

void Ideal_vector_multiplication_1(int party, NetIO* io, 
									IntFp *x, uint64_t len_x, uint64_t *y, uint64_t len_y, 
									uint64_t *ss_z, uint64_t *ss_mac_z, bool ss_zero)
{
	assert(len_x == len_y);
	// std::cout << "---------- Ideal_vector_multiplication running ------------" << std::endl;
	uint64_t len_z = 1;  // length of z is 1
	
	uint64_t *real_x = new uint64_t[len_x];
	uint64_t *real_mac_x = new uint64_t[len_x];
	uint64_t *mac_key_x = new uint64_t[len_x];
	memset(real_x, 0, len_x*sizeof(uint64_t));
	memset(real_mac_x, 0, len_x*sizeof(uint64_t));
	memset(mac_key_x, 0, len_x*sizeof(uint64_t));

	if(party == ALICE){
		for(uint64_t i=0; i< len_x; i++){
			real_x[i] = (uint64_t)HIGH64(x[i].value);
			real_mac_x[i] = (uint64_t)LOW64(x[i].value);
		}
		io->send_data(real_x, len_x*sizeof(uint64_t));
		io->send_data(real_mac_x, len_x*sizeof(uint64_t));
		if(ss_zero){
			std::memset(ss_z, 0, len_z*sizeof(uint64_t));
			std::memset(ss_mac_z, 0, len_z*sizeof(uint64_t));
		}else{
			io->recv_data(ss_z, len_z*sizeof(uint64_t));
			io->recv_data(ss_mac_z, len_z*sizeof(uint64_t));
		}
	}else{
		io->recv_data(real_x, len_x*sizeof(uint64_t));
		io->recv_data(real_mac_x, len_x*sizeof(uint64_t));

		uint64_t mac_delta = (uint64_t)LOW64(((ZKFpExecVer<NetIO> *)(ZKFpExec::zk_exec))->ostriple->delta);
		for(uint64_t i=0; i<len_x; i++){
			mac_key_x[i] = (uint64_t)LOW64(x[i].value);
			if((mult_mod(real_x[i], mac_delta) + mac_key_x[i])%prime_mod != real_mac_x[i]){
				error("Mac mult input check failed!\n");
			}
			uint64_t tmp = mult_mod(real_x[i], y[i]);
			*ss_z = (*ss_z + tmp)%prime_mod;
			*ss_mac_z = (*ss_mac_z + mult_mod(tmp, mac_delta))%prime_mod;
		}
		if(mult_mod(*ss_z, mac_delta) != *ss_mac_z){
			error("Mac mult output check failed!\n");
		}

		if(!ss_zero){
			PRG prg;
			uint64_t to_c_ss_z;
			uint64_t to_c_ss_mac_z;
			random_mod_p(prg, &to_c_ss_z, len_z, prime_mod);
			random_mod_p(prg, &to_c_ss_mac_z, len_z, prime_mod);
			io->send_data(&to_c_ss_z, len_z*sizeof(uint64_t));
			io->send_data(&to_c_ss_mac_z, len_z*sizeof(uint64_t));
			*ss_z = (*ss_z + prime_mod - to_c_ss_z)%prime_mod;
			*ss_mac_z = (*ss_mac_z + prime_mod - to_c_ss_mac_z)%prime_mod;
		}
	}
	// std::cout << "---------- Ideal_vector_multiplication finish ------------" << std::endl;
	io->flush();
	delete[] real_x;
	delete[] real_mac_x;
	delete[] mac_key_x;
}

// z = (x > 0)-----> x < prime_mod/2
void Ideal_vector_comparison(int party, NetIO* io, uint64_t *ss_x, uint64_t *ss_mac_x, uint64_t len_x,
								uint64_t *ss_z, uint64_t *ss_mac_z, uint64_t mac_delta, bool ss_zero)
{
	// std::cout << "---------- Ideal_vector_comparison running ------------" << std::endl;
	uint64_t len_z = len_x;
	if(party == ALICE){
		io->send_data(ss_x, len_x*sizeof(uint64_t));
		io->send_data(ss_mac_x, len_x*sizeof(uint64_t));
		if(ss_zero){
			std::memset(ss_z, 0, len_z*sizeof(uint64_t));
			std::memset(ss_mac_z, 0, len_z*sizeof(uint64_t));
		}else{
			io->recv_data(ss_z, len_z*sizeof(uint64_t));
			io->recv_data(ss_mac_z, len_z*sizeof(uint64_t));
		}
	}else{
		uint64_t *from_c_ss_x = new uint64_t[len_x];
		uint64_t *from_c_ss_mac_x = new uint64_t[len_x];
		io->recv_data(from_c_ss_x, len_x*sizeof(uint64_t));
		io->recv_data(from_c_ss_mac_x, len_x*sizeof(uint64_t));

		uint64_t *real_x = new uint64_t[len_x];
		uint64_t *real_mac_x = new uint64_t[len_x];
		for(uint64_t i=0; i<len_x; i++){
			real_x[i] = (from_c_ss_x[i] + ss_x[i])%prime_mod;
			real_mac_x[i] = (from_c_ss_mac_x[i] + ss_mac_x[i])%prime_mod;
			// std::cout << "real_x[i]: " << real_x[i] << std::endl;
			// std::cout << "prime_mod-real_x[i]: " << prime_mod - real_x[i] << std::endl;
			// std::cout << "real_mac_x[i]: " << real_mac_x[i] << std::endl;
			// std::cout << "mac_delta: " << mac_delta << std::endl;
			if(mult_mod(real_x[i], mac_delta) != real_mac_x[i]){
				error("Mac comparison input check failed!\n");
			}
			ss_z[i] = (real_x[i] < (prime_mod - 1)/2) ? 0 : 1;
			ss_mac_z[i] = (real_x[i] < (prime_mod - 1)/2) ? 0 : mac_delta;
		}
		if(!ss_zero){
			PRG prg;
			uint64_t *to_c_ss_z = new uint64_t[len_z];
			uint64_t *to_c_ss_mac_z = new uint64_t[len_z];
			random_mod_p(prg, to_c_ss_z, len_z, prime_mod);
			random_mod_p(prg, to_c_ss_mac_z, len_z, prime_mod);
			io->send_data(to_c_ss_z, len_z*sizeof(uint64_t));
			io->send_data(to_c_ss_mac_z, len_z*sizeof(uint64_t));
			for(uint64_t i=0; i<len_z; i++){
				ss_z[i] = (ss_z[i] + prime_mod - to_c_ss_z[i])%prime_mod;
				ss_mac_z[i] = (ss_mac_z[i] + prime_mod - to_c_ss_mac_z[i])%prime_mod;
			}
			delete[] to_c_ss_z;
			delete[] to_c_ss_mac_z;
		}
		delete[] from_c_ss_x;
		delete[] from_c_ss_mac_x;
		delete[] real_x;
		delete[] real_mac_x;
	}
	// std::cout << "---------- Ideal_vector_comparison finish ------------" << std::endl;
	io->flush();
}

void NormBall(int party, NetIO* io, IntFp *x, uint64_t len_x, uint64_t *y, uint64_t len_y, uint64_t bd_ln, uint64_t bd_l2, 
				uint64_t *ss_res, uint64_t *ss_mac_res, block *delta_blocks)
{
	// Server: BOB, Client: ALICE
	// [x] held by Client and Server, y held by Server(y[i] = 0 for client)
	// check l2_norm(vec(x) - vec(y)) < bd_l2 && ln_norm(vec(x)) < bd_ln
	assert(len_x == len_y);
	uint64_t len = len_x;

	std::cout << "---------- NormBall running ------------" << std::endl;

	// ln_norm(vec(x)) < bd_ln
	uint64_t start_comm = io->counter;
	auto start_time = clock_start();
	LnCheck(party, x, len_x, bd_ln);
	double ln_cost_time = time_from(start_time);
	uint64_t ln_cost_comm = (io->counter - start_comm);
	io->flush();
	// cout << "t1: " << (double)(time_from(start_time)) << endl;
	ln_time += ln_cost_time;
	ln_comm += ln_cost_comm;

	// compute h = x ^2 && commit h 
	uint64_t sum = 0;
	if(party == ALICE){
		for(uint64_t i=0; i<len; i++){
			sum += mult_mod((uint64_t)HIGH64(x[i].value), (uint64_t)HIGH64(x[i].value));
			// sum += 1;
		}
	}
	sum %= prime_mod;
	IntFp h = IntFp(sum, ALICE);
	// cout << "t1.5: " << (double)(time_from(start_time)) << endl;

	// verify h
	// new way
	fp_zkp_inner_prdt<BoolIO<NetIO>>(x, x, PR - sum, len_x);
	io->flush();
	// cout << "t2: " << (double)(time_from(start_time)) << endl;
	// old way
	// IntFp c_sum = IntFp(0, ALICE);
	// for(uint64_t i=0; i<len; i++){
	// 	c_sum = x[i] * x[i] + c_sum;
	// }
	// IntFp ver_h = h + c_sum.negate();
	// ver_h.reveal_zero();


	// share h, x, delta*h, delta*x   // sharing of h, x, delta*x are not neccesary, only need delta*h
	// PRG prg;
	uint64_t ss_h = 0;  // secret sharing of h
	uint64_t ss_mac_h = 0;   // secret sharing of delta*h: = M_h for server; = -K_h for client
	// uint64_t *ss_x =  new uint64_t[len];  // secret sharing of x
	// uint64_t *ss_mac_x = new uint64_t[len];   // secret sharing of delta*x: = M_x for Server; = -K_x for Client
	uint64_t mac_delta = 0;  // delta held by server, for client is 0
	if(party == ALICE){
		ss_h = (uint64_t)HIGH64(h.value);
		uint64_t mac_h = (uint64_t)LOW64(h.value);  // M_h
		ss_mac_h = mac_h; 
	}else{
		mac_delta = (uint64_t)LOW64(((ZKFpExecVer<NetIO> *)(ZKFpExec::zk_exec))->ostriple->delta);
		uint64_t mac_key_h = (uint64_t)LOW64(h.value);  // K_h
		ss_mac_h = (prime_mod - mac_key_h);
	}
	io->flush();
	// cout << "t3: " << (double)(time_from(start_time)) << endl;
	// std::cout << "ss_h: " << ss_h << std::endl;
	// std::cout << "prime_mod-ss_h: " << prime_mod-ss_h << std::endl;

	// compute vec(x) cdot vec(y)
	uint64_t ss_z = 0, ss_mac_z = 0;
	// Ideal_vector_multiplication(party, io, ss_x, ss_mac_x, len_x, y, len_y, &ss_z, &ss_mac_z, mac_delta);
	// Ideal_vector_multiplication_1(party, io, x, len_x, y, len_y, &ss_z, &ss_mac_z);
	vector_multiplication(party, io, x, len_x, y, len_y, &ss_z, &ss_mac_z);
	// std::cout << "ss_z: " << ss_z << std::endl;
	// std::cout << "prime_mod-ss_z: " << prime_mod-ss_z << std::endl;
	// std::cout << "ss_mac_z: " << ss_mac_z << std::endl;
	// cout << "t4: " << (double)(time_from(start_time)) << endl;
	// comparison: x^2 - 2xy > bd_l2 - y^2, where x^2 and 2xy are sharings, bd_l2 - y^2 can be computed by server
	
	uint64_t ss_tmp     = (ss_h     + prime_mod - (ss_z     + ss_z)     % prime_mod) % prime_mod;
	uint64_t ss_mac_tmp = (ss_mac_h + prime_mod - (ss_mac_z + ss_mac_z) % prime_mod) % prime_mod;
	uint64_t sqrt_y = 0; //compute y^2
	if(party == BOB){
		for(uint64_t i=0; i<len; i++) sqrt_y = (sqrt_y + mult_mod(y[i], y[i]))%prime_mod;
		// std::cout << "sqrt_y: " << sqrt_y << std::endl;
		// std::cout << "prime_mod-sqrt_y: " << prime_mod-sqrt_y << std::endl;
		uint64_t new_bd = (mult_mod(bd_l2, bd_l2) + prime_mod - sqrt_y)%prime_mod;
		ss_tmp = (ss_tmp + prime_mod - new_bd)%prime_mod;
		ss_mac_tmp = (ss_mac_tmp + prime_mod - mult_mod(new_bd, mac_delta))%prime_mod;
	}
	// std::cout << "ss_tmp: " << ss_tmp << std::endl;
	// std::cout << "prime_mod-ss_tmp: " << prime_mod-ss_tmp << std::endl;
	// std::cout << "ss_mac_tmp: " << ss_mac_tmp << std::endl;
	// Ideal_vector_comparison(party, io, &ss_tmp, &ss_mac_tmp, 1, ss_res, ss_mac_res, mac_delta);
	start_comm = io->counter;
	start_time = clock_start();
	Zk2Mpc(party, io, &ss_tmp, &ss_mac_tmp, 1, &mac_delta);
	v_time += time_from(start_time);
	v_comm += (io->counter - start_comm);
	io->flush();
	Wrap_comparison(party, 0, io, &ss_tmp, &ss_mac_tmp, 1, ss_res, ss_mac_res, mac_delta, delta_blocks);
	// std::cout << "*ss_res: " << *ss_res << std::endl;
	// std::cout << "*ss_mac_res: " << *ss_mac_res << std::endl;

	std::cout << "---------- NormBall finish ------------" << std::endl;
	io->flush();
}

void Ideal_L2Check(int party, IntFp *x, uint64_t len_x, uint64_t up_bd, uint64_t low_bd)
{
	uint64_t *real_x = new uint64_t[len_x];
	memset(real_x, 0, len_x*sizeof(uint64_t));
	if(party == ALICE){
		uint64_t sum = 0;
		for(uint64_t i=0; i< len_x; i++){
			real_x[i] = (uint64_t)HIGH64(x[i].value);
			sum += real_x[i];
		}
		if((sum <= low_bd*low_bd) && (sum >= up_bd*up_bd)) error("L2Check failed!");
	}
	delete[] real_x;
}

void CosSim(int party, NetIO* io, IntFp *x, uint64_t len_x, uint64_t *y, uint64_t len_y, uint64_t bd, uint64_t *ss_res, uint64_t *ss_mac_res, block *delta_blocks)
{
	// Server: BOB, Client: ALICE
	// [x] held by Client and Server, y held by Server (y[i] = 0 for client)
	// check cos(vec(x), vec(y)) > bd
	assert(len_x == len_y);
	uint64_t len = len_x;

	std::cout << "---------- CosSim running ------------" << std::endl;

	// ln_norm(vec(x)) < bound_ln
	uint64_t start_comm = io->counter;
	auto start_time = clock_start();
	LnCheck(party, x, len_x, bound_ln);
	double ln_cost_time = time_from(start_time);
	uint64_t ln_cost_comm = (io->counter - start_comm);
	// cout << "t1: " << (double)(time_from(start_time)) << endl;
	ln_time += ln_cost_time;
	ln_comm += ln_cost_comm;
	
	// for client: compute normalized vector h, h = x / ||x||_2 
	uint64_t *input_h = new uint64_t[len];
	memset(input_h, 0, len*sizeof(uint64_t));
	uint64_t *input = new uint64_t[len];
	double *real_input = new double[len];
	double input_cdot_input = 0;
	double l2_norm_input = 0;
	uint64_t l2_norm_input_field = 0;
	if(party == ALICE){
		for(uint64_t i=0; i<len; i++){
			input[i] = ((uint64_t)HIGH64(x[i].value))%prime_mod;
			real_input[i] = Field2Real(input[i], SCALE);
			input_cdot_input += real_input[i] * real_input[i];
		}
		l2_norm_input = sqrt(input_cdot_input);
		for(int i=0; i<len; i++){
			input_h[i] = Real2Field(real_input[i]/l2_norm_input, SCALE+HELP_SCALE);
		}
		l2_norm_input_field = Real2Field(l2_norm_input, SCALE+HELP_SCALE);
	}
	IntFp *h = new IntFp[len];
	for(int i=0; i<len; i++){
		h[i] = IntFp(input_h[i], ALICE);
	}
	IntFp c_l2_norm_input_field = IntFp(l2_norm_input_field, ALICE);
	// cout << "t1.5: " << (double)(time_from(start_time)) << endl;

	// verify h
	// new way (simulate to compute the overhead)
	IntFp *ver_h0 = new IntFp[len+1];
	IntFp *ver_h1 = new IntFp[len+1];
	for(int i=0; i<len; i++){
		ver_h0[i] = IntFp(uint64_t(1), ALICE);
		ver_h1[i] = IntFp(uint64_t(1), ALICE);
	}
	ver_h0[len] = IntFp(uint64_t(1), ALICE);
	ver_h1[len] = IntFp(uint64_t(1), PUBLIC);
	fp_zkp_inner_prdt<BoolIO<NetIO>>(ver_h0, ver_h1, 0, len+1);
	// cout << "t2: " << (double)(time_from(start_time)) << endl;

	// old way
	// IntFp *ver_h = new IntFp[len];
	// for(uint64_t i=0; i<len; i++){
	// 	ver_h[i] = x[i].negate() + h[i] * c_l2_norm_input_field;
	// }
	// batch_reveal_check_zero(ver_h, len);

	// rangecheck ||x|| in [0, bound_l2]
	IntFp tmp = IntFp(uint64_t(1));
	LUTRangeIntFp *LUTRange = new LUTRangeIntFp(party);
	uint64_t t = bound_l2%PR;
	LUTRange->LUTRangeinit(t);
	LUTRange->LUTRangeread(tmp);
	// LUTRange->LUTRangeread(c_l2_norm_input_field);
	delete LUTRange;
	
	// Check ||h||_2 = 1
	// Ideal_L2Check(party, h, len, (1<<(SCALE+HELP_SCALE))+allow_error, (1<<(SCALE+HELP_SCALE))-allow_error);
	start_comm = io->counter;
	start_time = clock_start();
	L2Check(party, h, len, (1<<(SCALE+HELP_SCALE))+allow_error, (1<<(SCALE+HELP_SCALE))-allow_error);
	double l2_cost_time = time_from(start_time);
	uint64_t l2_cost_comm = (io->counter - start_comm);
	l2_time += l2_cost_time;
	l2_comm += l2_cost_comm;
	
	
	// for server: compute normalized vector g, g = y / ||y||_2
	uint64_t *g = new uint64_t[len];
	memset(g, 0, len*sizeof(uint64_t));
	double *real_y = new double[len];
	double input_cdot_input_y = 0;
	double l2_norm_y = 0;
	if(party == BOB){
		for(uint64_t i=0; i<len; i++){
			real_y[i] = Field2Real(y[i], SCALE);
			input_cdot_input_y += real_y[i] * real_y[i];
		}
		l2_norm_y = sqrt(input_cdot_input_y);
		for(int i=0; i<len; i++){
			g[i] = Real2Field(real_y[i]/l2_norm_y, SCALE+HELP_SCALE);
		}
	}
	// cout << "t3: " << (double)(time_from(start_time)) << endl;
	
	// compute vec(x) cdot vec(y)
	uint64_t ss_z = 0, ss_mac_z = 0;
	// Ideal_vector_multiplication(party, io, ss_h, ss_mac_h, len, g, len, &ss_z, &ss_mac_z, mac_delta);  
	// Ideal_vector_multiplication_1(party, io, h, len, g, len, &ss_z, &ss_mac_z);			
	vector_multiplication(party, io, h, len, g, len, &ss_z, &ss_mac_z);
	io->flush();
	// std::cout << "ss_z: " << ss_z << std::endl;
	// std::cout << "ss_mac_z: " << ss_mac_z << std::endl;
	// Ideal_vector_truncate(party, io, &ss_z, &ss_mac_z, 1, &ss_z, &ss_mac_z, SCALE, mac_delta);
	// cout << "t4: " << (double)(time_from(start_time)) << endl;

	// comparison
	uint64_t mac_delta = 0;
	uint64_t ss_tmp     = (prime_mod - ss_z)     % prime_mod;  // -ss_z
	uint64_t ss_mac_tmp = (prime_mod - ss_mac_z) % prime_mod;  // -ss_mac_z
	uint64_t new_bd = mult_mod(bd, (uint64_t)1<<(SCALE+2*HELP_SCALE));  
	if(party == BOB){
		mac_delta = (uint64_t)LOW64(((ZKFpExecVer<NetIO> *)(ZKFpExec::zk_exec))->ostriple->delta);
		ss_tmp = (ss_tmp + new_bd)%prime_mod;
		ss_mac_tmp = (ss_mac_tmp + mult_mod(new_bd, mac_delta))%prime_mod;
	}
	// std::cout << "ss_tmp: " << ss_tmp << std::endl;
	// std::cout << "ss_mac_tmp: " << ss_mac_tmp << std::endl;
	// Ideal_vector_comparison(party, io, &ss_tmp, &ss_mac_tmp, 1, ss_res, ss_mac_res, mac_delta);
	start_comm = io->counter;
	start_time = clock_start();
	Zk2Mpc(party, io, &ss_tmp, &ss_mac_tmp, 1, &mac_delta);
	v_time += time_from(start_time);
	v_comm += (io->counter - start_comm);
	Wrap_comparison(party, 0, io, &ss_tmp, &ss_mac_tmp, 1, ss_res, ss_mac_res, mac_delta, delta_blocks);
	// std::cout << "*ss_res: " << *ss_res << std::endl;
	// std::cout << "*ss_mac_res: " << *ss_mac_res << std::endl;
	std::cout << "---------- CosSim finish ------------" << std::endl;
	io->flush();
	delete[] input_h;
	delete[] input;
	delete[] real_input;
	delete[] h;
	delete[] ver_h0;
	delete[] ver_h1;
	delete[] g;
	delete[] real_y;
}

void Ideal_vector_multiplication_2(int party, NetIO* io, 
									IntFp *x, uint64_t len_x, uint64_t y, uint64_t mac_delta,
									uint64_t *ss_z, uint64_t *ss_mac_z)
{
	// [x] held by client and server, y held by server, compute <xy> and <delta*xy>
	uint64_t *input_x = new uint64_t[len_x];
	memset(input_x, 0, len_x*sizeof(uint64_t));
	if(party == ALICE){
		for(uint64_t i=0; i<len_x; i++){
			input_x[i] = (uint64_t)HIGH64(x[i].value);
		}
		io->send_data(input_x, len_x*sizeof(uint64_t));
		memset(ss_z, 0, len_x*sizeof(uint64_t));
		memset(ss_mac_z, 0, len_x*sizeof(uint64_t));
	}else{
		io->recv_data(input_x, len_x*sizeof(uint64_t));
		if(y == 0){ // y is either 0 or 1
			memset(ss_z, 0, len_x*sizeof(uint64_t));
			memset(ss_mac_z, 0, len_x*sizeof(uint64_t));
		}else{
			memcpy(ss_z, input_x, len_x*sizeof(uint64_t));
			for(uint64_t i=0; i<len_x; i++){
				ss_mac_z[i] = mult_mod(input_x[i], mac_delta);
			}
		}
	}
	io->flush();
	delete[] input_x;
}

void GradFilter(int party, NetIO* io, IntFp *x, uint64_t len_x, uint64_t *y, uint64_t len_y, uint64_t bd, uint64_t *ss_res, uint64_t *ss_mac_res){
	// a simple test: if x[0]%2==0, then set *ss_res=1, *ss_mac_res=delta; otherwise *ss_res=0, *ss_mac_res=0.
	uint64_t mac_delta = 0;
	if(party == BOB){
		mac_delta = (uint64_t)LOW64(((ZKFpExecVer<NetIO> *)(ZKFpExec::zk_exec))->ostriple->delta);
		io->send_data(&mac_delta, sizeof(uint64_t));
		*ss_res = 0;
		*ss_mac_res = 0;
	}else{
		io->recv_data(&mac_delta, sizeof(uint64_t));
		*ss_res = (((uint64_t)HIGH64(x[0].value))%2==0) ? 1 : 0;
		*ss_mac_res = (((uint64_t)HIGH64(x[0].value))%2==0) ? mac_delta : 0;
	}
	io->flush();
}

void NormFilter(int party, NetIO* io, IntFp *x, uint64_t len_x, uint64_t *y, uint64_t len_y, uint64_t bd_ln, uint64_t bd_l2, 
                uint64_t *ss_res, uint64_t *ss_mac_res, block *delta_blocks){
	assert(len_x == len_y);
	uint64_t len = len_x;

	std::cout << "---------- NormFiler running ------------" << std::endl;

	// ln_norm(vec(x)) < bound_ln
	uint64_t start_comm = io->counter;
	auto start_time = clock_start();
	LnCheck(party, x, len_x, bd_ln);
	double ln_cost_time = time_from(start_time);
	uint64_t ln_cost_comm = (io->counter - start_comm);
	ln_time += ln_cost_time;
	ln_comm += ln_cost_comm;

	start_comm = io->counter;
	start_time = clock_start();
	L2Check(party, x, len, bd_l2, 0);
	double l2_cost_time = time_from(start_time);
	uint64_t l2_cost_comm = (io->counter - start_comm);
	l2_time += l2_cost_time;
	l2_comm += l2_cost_comm;

	uint64_t mac_delta = 0;
	if(party == BOB){
		mac_delta = (uint64_t)LOW64(((ZKFpExecVer<NetIO> *)(ZKFpExec::zk_exec))->ostriple->delta);
	}
	*ss_res = (party == ALICE) ? 0 : 1;
	*ss_mac_res = *ss_res * mac_delta;
	std::cout << "---------- NormFilter finish ------------" << std::endl;
	io->flush();
}

void Aggregation(int party, NetIO* io, uint64_t **x, uint64_t num_client, uint64_t len_x, uint64_t *y, uint64_t len_y, uint64_t *z, block *delta_blocks, int FilterType){
	// x are the gradients held by clients, x[i] is held by client i, num_client is the number of clients
	// y is the gradient held by server
	// len_x, len_y is the dimension of gradients
	// z is the result of the aggregation
	assert(len_x == len_y);
	uint64_t len = len_x;

	PRG prg;
	uint64_t **rand_for_ss_zero = new uint64_t*[num_client];
	uint64_t **rand_for_ss_mac_zero = new uint64_t*[num_client];
	for(uint64_t i=0; i<num_client; i++){
		rand_for_ss_zero[i] = new uint64_t[len];
		rand_for_ss_mac_zero[i] = new uint64_t[len];
		random_mod_p(prg, rand_for_ss_zero[i], len, prime_mod);
		random_mod_p(prg, rand_for_ss_mac_zero[i], len, prime_mod);
	}

	IntFp **input_x = new IntFp*[num_client];
	uint64_t mac_delta = 0;

	uint64_t *weight_x = new uint64_t[num_client];
	memset(weight_x, 0, num_client*sizeof(uint64_t));
	uint64_t *mac_weight_x = new uint64_t[num_client];
	memset(mac_weight_x, 0, num_client*sizeof(uint64_t));

	uint64_t **weighted_gradient = new uint64_t*[num_client];
	uint64_t **mac_weighted_gradient = new uint64_t*[num_client];
	uint64_t **from_c_weighted_gradient = new uint64_t*[num_client];
	uint64_t **from_c_mac_weighted_gradient = new uint64_t*[num_client];

	uint64_t *aggregated_result = new uint64_t[len];
	memset(aggregated_result, 0, len*sizeof(uint64_t));
	uint64_t *mac_aggregated_result = new uint64_t[len];
	memset(mac_aggregated_result, 0, len*sizeof(uint64_t));

	for(uint64_t i=0; i<num_client; i++){
		std::cout << "\n--------------- Client " << i << " Check running -----------------" << std::endl;
		// commit
		input_x[i] = new IntFp[len];
		for(uint64_t j=0; j<len; j++){
			input_x[i][j] = IntFp(x[i][j], ALICE);
		}
		if(party == BOB){
			mac_delta = (uint64_t)LOW64(((ZKFpExecVer<NetIO> *)(ZKFpExec::zk_exec))->ostriple->delta);
		}

		// compute the weight of x
		uint64_t tmp_weight=0, tmp_mac_weight=0;
		if(FilterType==0){
			GradFilter(party, io, input_x[i], len, y, len, bound_cos, &tmp_weight, &tmp_mac_weight);
		}
		else if(FilterType==1){
			NormBall(party, io, input_x[i], len, y, len, bound_ln, bound_l2, &tmp_weight, &tmp_mac_weight, delta_blocks);
		}
		else if(FilterType==2){
			CosSim(party, io, input_x[i], len, y, len, bound_cos, &tmp_weight, &tmp_mac_weight, delta_blocks);
		}
		else{
			NormFilter(party, io, input_x[i], len, y, len, bound_ln, bound_l2, &tmp_weight, &tmp_mac_weight, delta_blocks);
		}

		uint64_t real_weight=0, real_mac_weight=0;
		if(party==ALICE){
			io->send_data(&tmp_weight, sizeof(uint64_t));
			io->send_data(&tmp_mac_weight, sizeof(uint64_t));
		}else{
			uint64_t weight_from_c=0, mac_weight_from_c=0;
			io->recv_data(&weight_from_c, sizeof(uint64_t));
			io->recv_data(&mac_weight_from_c, sizeof(uint64_t));
			real_weight = (weight_from_c + tmp_weight)%prime_mod;
			real_mac_weight = (mac_weight_from_c + tmp_mac_weight)%prime_mod;
			if(mult_mod(mac_delta, real_weight) != real_mac_weight){  // client i modify its weight share, abort!
				error("Abort! There are malicious clients!\n");
				// real_weight = 0;
				// real_mac_weight = 0;
			}
			weight_x[i] = real_weight;
			mac_weight_x[i] = real_mac_weight;
		}
		io->flush();

		// compute weighted gradients: <weight_x * x> and <delta*weight_x * x>
		weighted_gradient[i] = new uint64_t[len];
		memset(weighted_gradient[i], 0, len*sizeof(uint64_t));
		mac_weighted_gradient[i] = new uint64_t[len];
		memset(mac_weighted_gradient[i], 0, len*sizeof(uint64_t));
		// Ideal_vector_multiplication_2(party, io, input_x[i], len, real_weight, mac_delta, weighted_gradient[i], mac_weighted_gradient[i]);
		vector_bool_multiplication(party, io, input_x[i], len, real_weight, weighted_gradient[i], mac_weighted_gradient[i]); 

		// clients mask weighted gradients and send them to server
		if(party == ALICE){
			for(uint64_t j=0; j<len; j++){
				weighted_gradient[i][j] = (weighted_gradient[i][j] + rand_for_ss_zero[i][j] + prime_mod - rand_for_ss_zero[(i+1)%num_client][j])%prime_mod;
				mac_weighted_gradient[i][j] = (mac_weighted_gradient[i][j] + rand_for_ss_mac_zero[i][j] + prime_mod - rand_for_ss_mac_zero[(i+1)%num_client][j])%prime_mod;
			}
			io->send_data(weighted_gradient[i], len*sizeof(uint64_t));
			io->send_data(mac_weighted_gradient[i], len*sizeof(uint64_t));
		}else{
			from_c_weighted_gradient[i] = new uint64_t[len];
			from_c_mac_weighted_gradient[i] = new uint64_t[len];
			io->recv_data(from_c_weighted_gradient[i], len*sizeof(uint64_t));
			io->recv_data(from_c_mac_weighted_gradient[i], len*sizeof(uint64_t));
		}
		io->flush();
	}
	std::cout << "\n--------------- Aggregetion running -----------------" << std::endl;
	if(party == BOB){
		uint64_t *from_c_aggregation = new uint64_t[len];
		uint64_t *from_c_mac_aggregation = new uint64_t[len];
		memset(from_c_aggregation, 0, len*sizeof(uint64_t));
		memset(from_c_mac_aggregation, 0, len*sizeof(uint64_t));

		uint64_t *from_s_aggregation = new uint64_t[len];
		uint64_t *from_s_mac_aggregation = new uint64_t[len];
		memset(from_s_aggregation, 0, len*sizeof(uint64_t));
		memset(from_s_mac_aggregation, 0, len*sizeof(uint64_t));

		// sum(from_c_weighted_gradient), sum(from_c_mac_weighted_gradient)
		for(uint64_t i=0; i<num_client; i++){
			for(uint64_t j=0; j<len; j++){
				from_c_aggregation[j] = (from_c_aggregation[j] + from_c_weighted_gradient[i][j])%prime_mod;
				from_c_mac_aggregation[j] = (from_c_mac_aggregation[j] + from_c_mac_weighted_gradient[i][j])%prime_mod;
				from_s_aggregation[j] = (from_s_aggregation[j] + weighted_gradient[i][j])%prime_mod;
				from_s_mac_aggregation[j] = (from_s_mac_aggregation[j] + mac_weighted_gradient[i][j])%prime_mod;
			}
		}

		// verify: delta * aggregation == mac_aggregation
		uint64_t ctr = 0;
		for(uint64_t i=0; i<len; i++){
			aggregated_result[i] = (from_c_aggregation[i]+from_s_aggregation[i])%prime_mod;
			mac_aggregated_result[i] = (from_c_mac_aggregation[i]+from_s_mac_aggregation[i])%prime_mod;
			if(mult_mod(aggregated_result[i], mac_delta) == mac_aggregated_result[i]){
				ctr++;
			}
		}
		if(ctr < len){
			error("Check failed! Some clients modify their sharings of weighted gradients!\n");
		}
		// send aggregation result to clients?
		delete[] from_c_aggregation;
		delete[] from_c_mac_aggregation;
		delete[] from_s_aggregation;
		delete[] from_s_mac_aggregation;
	}
	memcpy(z, aggregated_result, len*sizeof(uint64_t));
	io->flush();

	for(uint64_t i=0; i<num_client; i++){
		delete[] rand_for_ss_zero[i];
		delete[] rand_for_ss_mac_zero[i];
		delete[] input_x[i];
		delete[] weighted_gradient[i];
		delete[] mac_weighted_gradient[i];
		if(party == BOB){
			delete[] from_c_weighted_gradient[i];
			delete[] from_c_mac_weighted_gradient[i];
		}
	}
	delete[] rand_for_ss_zero;
	delete[] rand_for_ss_mac_zero;
	delete[] input_x;
	delete[] weight_x;
	delete[] mac_weight_x;
	delete[] weighted_gradient;
	delete[] mac_weighted_gradient;
	delete[] from_c_weighted_gradient;
	delete[] from_c_mac_weighted_gradient;
	delete[] aggregated_result;
	delete[] mac_aggregated_result;
}

