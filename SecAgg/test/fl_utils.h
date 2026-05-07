#ifndef FL_UTILS_H__
#define FL_UTILS_H__

// #include "emp-sh2pc/emp-sh2pc.h"
#include "emp-zk/emp-zk-arith/emp-zk-arith.h"
#include "emp-zk/emp-zk-arith/polynomial.h"
#include "emp-zk/emp-zk-math/ZKmath-functions.h"
#include "emp-zk/emp-zk.h"
#include "emp-zk/emp-vole/utility.h"

#include "emp-sh2pc/sh_gen.h"
#include "emp-sh2pc/sh_eva.h"

#include "seal/util/uintarith.h"
#include "seal/util/uintarithsmallmod.h"

#include "LinearLayer/fc-field.h"
#include "LinearLayer/defines-HE.h"

// #include <iostream>
// #include <cmath>
// #include <thread>

#define HELP_SCALE 0   

// fl_comp
#define SHL128(v, n) \
({ \
    __m128i v1, v2; \
 \
    if ((n) >= 64) \
    { \
        v1 = _mm_slli_si128(v, 8); \
        v1 = _mm_slli_epi64(v1, (n) - 64); \
    } \
    else \
    { \
        v1 = _mm_slli_epi64(v, n); \
        v2 = _mm_slli_si128(v, 8); \
        v2 = _mm_srli_epi64(v2, 64 - (n)); \
        v1 = _mm_or_si128(v1, v2); \
    } \
    v1; \
})

extern uint64_t prime_mod;
extern int32_t bitlength;
extern int num_threads;
extern uint64_t verify;

// normball
extern block block_default;
// cossim
extern uint64_t bound_cos;
extern uint64_t bound_ln;
extern uint64_t bound_l2;
extern uint64_t allow_error;
extern uint64_t ncheck;

// overhead
extern uint64_t s_comm;
extern uint64_t c_comm;

extern uint64_t v_comm;
extern uint64_t swap_comm;
extern uint64_t l2_comm;
extern uint64_t ln_comm;
extern uint64_t mult_comm;
extern uint64_t comp_comm;

extern double v_time;
extern double swap_time;
extern double l2_time;
extern double ln_time;
extern double mult_time;
extern double comp_time;
extern double wait_time;

extern FCField he_fc;

// fl_mult
void Zk2Mpc(int party, NetIO *io, IntFp *x, uint64_t len_x, uint64_t *y, uint64_t len_y, uint64_t *x_value, uint64_t *mac_M, uint64_t *delta, uint64_t *mac_K);
// fl_comp
void Zk2Mpc(int party, NetIO *io, uint64_t *x, uint64_t *mac_x, uint64_t len_x, uint64_t *delta);
// void LinearLayerFC(int party, NetIO *io, uint64_t common_dim); 
void vector_multiplication(int party, NetIO* io, IntFp *x, uint64_t len_x, 
							uint64_t *y, uint64_t len_y, uint64_t *ss_z, uint64_t *ss_mac_z);
void vector_bool_multiplication(int party, NetIO* io, IntFp *x, 
							uint64_t len_x, uint64_t y, uint64_t *ss_z, uint64_t *ss_mac_z);

// fl_comp
uint64_t mod_shift(uint64_t a, uint64_t b, uint64_t prime_mod);
void pack_decryption_table(uint64_t *pack_table, uint64_t *ciphertexts, int pack_size, int batch_size, int bitlen);
void unpack_decryption_table(uint64_t *pack_table, uint64_t *ciphertexts, int pack_size, int batch_size, int bitlen);
void create_ciphertexts(Integer *garbled_data, block label_delta, uint64_t *ciphertexts, uint64_t* server_shares, 
                        int bitlen, uint64_t nrelu, uint64_t alpha, int l_idx, bool apply_prg);
void decrypt_ciphertexts(Integer *garbled_data, uint64_t *ciphertexts, uint64_t* client_shares, int bitlen, uint64_t nrelu, int l_idx, bool apply_prg);
void comparison(int party, int tid, NetIO* io, uint64_t* inputs, uint64_t nrelu, 
                uint64_t* ip_ss, uint64_t* op_ss, uint64_t* op_mss, uint64_t mac_key, block *delta_blocks, int bitlen=bitlength);
void Wrap_comparison(int party, int tid, NetIO* io, uint64_t *ss_x, uint64_t *ss_mac_x, uint64_t len_x,
					uint64_t *ss_z, uint64_t *ss_mac_z, uint64_t mac_delta, block *delta_blocks, bool ss_zero=false);

// fl_l2check  fl_lncheck  fl_normball  fl_cossim
uint64_t comm(BoolIO<NetIO> **ios, int num_threads=8);

// fl_l2check
void L2Check(int party, IntFp *x, uint64_t len_x, uint64_t up_bd, uint64_t low_bd=0);
// fl_lncheck
void LnCheck(int party, IntFp *x, uint64_t len_x, uint64_t bd);

// fl_normball  fl_cossim  fl_aggregation
void Ideal_vector_multiplication(int party, NetIO* io, uint64_t *ss_x, uint64_t *ss_mac_x, uint64_t len_x, 
									uint64_t *y, uint64_t len_y, 
									uint64_t *ss_z, uint64_t *ss_mac_z, uint64_t mac_delta, bool ss_zero = false);
void Ideal_vector_multiplication_1(int party, NetIO* io, 
									IntFp *x, uint64_t len_x, uint64_t *y, uint64_t len_y, 
									uint64_t *ss_z, uint64_t *ss_mac_z, bool ss_zero = false);
void Ideal_vector_comparison(int party, NetIO* io, uint64_t *ss_x, uint64_t *ss_mac_x, uint64_t len_x,
								uint64_t *ss_z, uint64_t *ss_mac_z, uint64_t mac_delta, bool ss_zero = false);

// fl_normball
void NormBall(int party, NetIO* io, IntFp *x, uint64_t len_x, uint64_t *y, uint64_t len_y, uint64_t bd_ln, uint64_t bd_l2, 
                uint64_t *ss_res, uint64_t *ss_mac_res, block *delta_blocks=&block_default);

// fl_cossim
void Ideal_L2Check(int party, IntFp *x, uint64_t len_x, uint64_t up_bd, uint64_t low_bd=0);
void CosSim(int party, NetIO* io, IntFp *x, uint64_t len_x, uint64_t *y, uint64_t len_y, uint64_t bd, 
            uint64_t *ss_res, uint64_t *ss_mac_res, block *delta_blocks=&block_default);

// l2+ln
void NormFilter(int party, NetIO* io, IntFp *x, uint64_t len_x, uint64_t *y, uint64_t len_y, uint64_t bd_ln, uint64_t bd_l2, 
                uint64_t *ss_res, uint64_t *ss_mac_res, block *delta_blocks=&block_default);

// fl_aggregation
void Ideal_vector_multiplication_2(int party, NetIO* io, 
									IntFp *x, uint64_t len_x, uint64_t y, uint64_t mac_delta,
									uint64_t *ss_z, uint64_t *ss_mac_z);  // bool *vector
void GradFilter(int party, NetIO* io, IntFp *x, uint64_t len_x, uint64_t *y, uint64_t len_y, uint64_t bd, uint64_t *ss_res, uint64_t *ss_mac_res);
void Aggregation(int party, NetIO* io, uint64_t **x, uint64_t num_client, uint64_t len_x, uint64_t *y, uint64_t len_y, uint64_t *z, block *delta_blocks=&block_default, int FilterType=0);

#endif