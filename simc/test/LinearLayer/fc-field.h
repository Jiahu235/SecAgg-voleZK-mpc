/*
Original Author: ryanleh
Modified Work Copyright (c) 2020 Microsoft Research

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.

Modified by Deevashwer Rathee
*/

#ifndef FC_FIELD_H__
#define FC_FIELD_H__

#include <iostream>
#include <vector>
#include "utils-HE.h"
#include "emp-zk/emp-zk.h"

struct FCMetadata {
  uint64_t slot_count;
  uint64_t pack_num;
  uint64_t inp_ct;
  uint64_t inp_ct_1;
  // Filter is a matrix
  uint64_t filter_h;
  uint64_t filter_w;
  uint64_t filter_size;
  // Image is a vector
  uint64_t image_size;
};

std::vector<seal::Ciphertext> CutArrs(std::vector<seal::Ciphertext>& Arrs, int begin, int end);

seal::Ciphertext preprocess_vec(const uint64_t *input, const FCMetadata &data,
                                seal::Encryptor &encryptor,
                                seal::BatchEncoder &batch_encoder);
seal::Plaintext preprocess_vec_plain(const uint64_t *input, const FCMetadata &data,
                                seal::BatchEncoder &batch_encoder);
std::vector<seal::Plaintext>
preprocess_matrix(const uint64_t *const *matrix, const FCMetadata &data,
                  seal::BatchEncoder &batch_encoder);

std::vector<seal::Plaintext> preprocess_matrix_1(const uint64_t *const *matrix,
                                    const FCMetadata &data,
                                    seal::BatchEncoder &batch_encoder);

seal::Plaintext fc_preprocess_noise(const uint64_t *secret_share,
                                     const FCMetadata &data,
                                     seal::BatchEncoder &batch_encoder);

seal::Ciphertext fc_online(seal::Ciphertext &ct,
                           std::vector<seal::Plaintext> &enc_mat,
                           const FCMetadata &data, seal::Evaluator &evaluator,
                           seal::GaloisKeys &gal_keys, seal::RelinKeys &relin_keys, seal::Ciphertext &zero);

seal::Ciphertext fc_online_1(seal::Ciphertext &ct, std::vector<seal::Plaintext> &enc_mat,
                     const FCMetadata &data, seal::Evaluator &evaluator,
                     seal::GaloisKeys &gal_keys, seal::RelinKeys &relin_keys, seal::Ciphertext &zero);

uint64_t *fc_postprocess(seal::Ciphertext &result, const FCMetadata &data,
                         seal::BatchEncoder &batch_encoder,
                         seal::Decryptor &decryptor);

uint64_t *fc_postprocess_1(seal::Ciphertext &ct, const FCMetadata &data,
                         seal::BatchEncoder &batch_encoder, seal::Decryptor &decryptor);

uint64_t *fc_postprocess_mac(seal::Ciphertext &result, const FCMetadata &data,
                        seal::BatchEncoder &batch_encoder,
                        seal::Decryptor &decryptor);

class FCField {
public:
  int party;
  emp::NetIO *io;
  emp::BoolIO<emp::NetIO> **ios;
  uint64_t num_threads;
  FCMetadata data;
  FCMetadata data_1;
  std::shared_ptr<seal::SEALContext> context;
  seal::Encryptor *encryptor;
  seal::Decryptor *decryptor;
  seal::Evaluator *evaluator;
  seal::BatchEncoder *encoder;
  seal::GaloisKeys *gal_keys;
  seal::RelinKeys *relin_keys;
  seal::Ciphertext *zero;
  size_t slot_count;

  FCField();
  FCField(int party, emp::NetIO *io);
  FCField(int party, emp::NetIO *io, emp::BoolIO<emp::NetIO> **ios, int num_threads);

  ~FCField();

  void init(int party, emp::NetIO *io);
  void init(int party, emp::NetIO *io, emp::BoolIO<emp::NetIO> **ios, int num_threads);

  void configure();

  void configure_1();

  std::vector<uint64_t> ideal_functionality(uint64_t *vec, uint64_t **matrix, seal::Modulus mod);
  std::vector<uint64_t> ideal_functionality(uint64_t *vec, uint64_t **matrix, uint64_t rows, uint64_t common_dim, seal::Modulus mod);

  void matrix_multiplication(int32_t num_rows, int32_t common_dim,
                             int32_t num_cols,
                             std::vector<std::vector<uint64_t>> &A,
                             std::vector<std::vector<uint64_t>> &B,
                             std::vector<std::vector<uint64_t>> &C,
                             bool verify_output = false, bool verbose = false);
 void matrix_multiplication_first(int32_t num_rows, int32_t common_dim,
                            int32_t num_cols,
                            std::vector<std::vector<uint64_t>> &inputs,
                            std::vector<std::vector<uint64_t>> &op_shares,
                            std::vector<std::vector<uint64_t>> &mac_op_shares,
                            seal::Modulus mod,
                            bool verify_output = false, bool verbose = false);

  void matrix_multiplication_gen(int32_t num_rows, int32_t common_dim,
                             int32_t num_cols,
                             std::vector<std::vector<uint64_t>> &matrix,
                             std::vector<std::vector<uint64_t>> &input_share,
                             std::vector<std::vector<uint64_t>> &mac_input_share,
                             seal::Modulus mod,
                             bool verify_output = true, bool verbose = false);

  void vector_multiplication(uint64_t num_rows, uint64_t common_dim,
                                    uint64_t num_cols,
                                    vector<vector<uint64_t>> &matrix,
                                    vector<vector<uint64_t>> &input_share,
                                    vector<vector<uint64_t>> &mac_input_share,
                                    uint64_t mac_key,
                                    seal::Modulus mod,
                                    bool verify_output, bool verbose);

  void vector_multiplication_large(uint64_t num_rows, uint64_t common_dim,
                                    uint64_t num_cols,
                                    vector<vector<uint64_t>> &matrix,
                                    vector<vector<uint64_t>> &input_share,
                                    vector<vector<uint64_t>> &mac_input_share,
                                    uint64_t mac_key,
                                    seal::Modulus mod,
                                    bool verify_output, bool verbose);

  void vector_multiplication_large_no_share(uint64_t num_rows, uint64_t common_dim, uint64_t num_cols,
                                    vector<vector<uint64_t>> &matrix,
                                    vector<vector<uint64_t>> &input,
                                    vector<vector<uint64_t>> &mac_M,
                                    vector<vector<uint64_t>> &mac_K,
                                    uint64_t mac_key,
                                    uint64_t *output,
                                    uint64_t *output_mac,
                                    seal::Modulus mod,
                                    bool verify_output, bool verbose);

  void vector_multiplication_large_no_share_1(uint64_t num_rows, uint64_t common_dim, uint64_t num_cols,
                                    vector<vector<uint64_t>> &matrix,
                                    vector<vector<uint64_t>> &input,
                                    vector<vector<uint64_t>> &mac_M,
                                    vector<vector<uint64_t>> &mac_K,
                                    uint64_t mac_key,
                                    uint64_t *output,
                                    uint64_t *output_mac,
                                    seal::Modulus mod,
                                    bool verify_output, bool verbose, double &wait_time);
  
  void vector_bool_multiplication_large_no_share(uint64_t num_rows, uint64_t common_dim, uint64_t num_cols,
                                    uint64_t mux,
                                    vector<vector<uint64_t>> &input,
                                    vector<vector<uint64_t>> &mac_M,
                                    vector<vector<uint64_t>> &mac_K,
                                    uint64_t mac_key,
                                    uint64_t *output,
                                    uint64_t *output_mac,
                                    seal::Modulus mod,
                                    bool verify_output, bool verbose, double &wait_time);

  void verify(std::vector<uint64_t> *vec, std::vector<uint64_t *> *matrix,
              std::vector<std::vector<uint64_t>> &C);
};
#endif
