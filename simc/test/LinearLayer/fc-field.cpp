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

#include "fc-field.h"

using namespace std;
using namespace seal;
using namespace emp;

vector<Ciphertext> CutArrs(vector<Ciphertext>& Arrs, int begin, int end){
  vector<Ciphertext> result;
  result.assign(Arrs.begin()+ begin, Arrs.begin()+ end);
  return result;
}

Ciphertext preprocess_vec(const uint64_t *input, const FCMetadata &data,
                          Encryptor &encryptor, BatchEncoder &batch_encoder) {
  // Create copies of the input vector to fill the ciphertext appropiately.
  // Pack using powers of two for easy rotations later
  vector<uint64_t> pod_matrix(data.slot_count, 0ULL);
  uint64_t size_pow2 = next_pow2(data.image_size);
  #pragma omp parallel for num_threads(num_threads) schedule(static)
  for (uint64_t col = 0; col < data.image_size; col++) {
    for (uint64_t idx = 0; idx < data.pack_num; idx++) {
      pod_matrix[col + size_pow2 * idx] = input[col];
    }
  }

  Ciphertext ciphertext;
  Plaintext tmp;
  batch_encoder.encode(pod_matrix, tmp);
  encryptor.encrypt(tmp, ciphertext);
  return ciphertext;
}

Plaintext preprocess_vec_plain(const uint64_t *input, const FCMetadata &data,
                          BatchEncoder &batch_encoder) {
  // Create copies of the input vector to fill the ciphertext appropiately.
  // Pack using powers of two for easy rotations later
  vector<uint64_t> pod_matrix(data.slot_count, 0ULL);
  uint64_t size_pow2 = next_pow2(data.image_size);
  #pragma omp parallel for num_threads(num_threads) schedule(static)
  for (uint64_t col = 0; col < data.image_size; col++) {
    for (uint64_t idx = 0; idx < data.pack_num; idx++) {
      pod_matrix[col + size_pow2 * idx] = input[col];
    }
  }

  Plaintext plaintext;
  batch_encoder.encode(pod_matrix, plaintext);
  return plaintext;
}

vector<Plaintext> preprocess_matrix(const uint64_t *const *matrix,
                                    const FCMetadata &data,
                                    BatchEncoder &batch_encoder) {
  // Pack the filter in alternating order of needed ciphertexts. This way we
  // rotate the input once per ciphertext
  vector<vector<uint64_t>> mat_pack(data.inp_ct,
                                    vector<uint64_t>(data.slot_count, 0ULL));
  #pragma omp parallel for num_threads(num_threads) schedule(static)
  for (uint64_t row = 0; row < data.filter_h; row++) {
    uint64_t ct_idx = row / data.inp_ct;
    for (uint64_t col = 0; col < data.filter_w; col++) {
      mat_pack[row % data.inp_ct][col + next_pow2(data.filter_w) * ct_idx] =
          matrix[row][col];
    }
  }

  // Take the packed ciphertexts above and repack them in a diagonal ordering.
  uint64_t mod_mask = (data.inp_ct - 1);
  uint64_t wrap_thresh = min(data.slot_count >> 1, next_pow2(data.filter_w));
  uint64_t wrap_mask = wrap_thresh - 1;
  vector<vector<uint64_t>> mat_diag(data.inp_ct,
                                    vector<uint64_t>(data.slot_count, 0ULL));
  #pragma omp parallel for num_threads(num_threads) schedule(static)
  for (uint64_t ct = 0; ct < data.inp_ct; ct++) {
    for (uint64_t col = 0; col < data.slot_count; col++) {
      uint64_t ct_diag_l = (col - ct) & wrap_mask & mod_mask;
      uint64_t ct_diag_h = (col ^ ct) & (data.slot_count / 2) & mod_mask;
      uint64_t ct_diag = (ct_diag_h + ct_diag_l);

      uint64_t col_diag_l = (col - ct_diag_l) & wrap_mask;
      uint64_t col_diag_h = wrap_thresh * (col / wrap_thresh) ^ ct_diag_h;
      uint64_t col_diag = col_diag_h + col_diag_l;

      mat_diag[ct_diag][col_diag] = mat_pack[ct][col];
    }
  }

  vector<Plaintext> enc_mat(data.inp_ct);
  #pragma omp parallel for num_threads(num_threads) schedule(static)
  for (uint64_t ct = 0; ct < data.inp_ct; ct++) {
    batch_encoder.encode(mat_diag[ct], enc_mat[ct]);
  }
  return enc_mat;
}

vector<Plaintext> preprocess_matrix_1(const uint64_t *const *matrix,
                                    const FCMetadata &data,
                                    BatchEncoder &batch_encoder) {
  // Pack the filter in alternating order of needed ciphertexts. This way we
  // rotate the input once per ciphertext
  vector<vector<uint64_t>> mat_pack(data.inp_ct,
                                    vector<uint64_t>(data.slot_count, 0ULL));
  for (uint64_t row = 0; row < data.filter_h; row++) {
    uint64_t ct_idx = row / data.inp_ct;
    #pragma omp parallel for num_threads(num_threads) schedule(static)
    for (uint64_t col = 0; col < data.filter_w; col++) {
      mat_pack[row % data.inp_ct][col + next_pow2(data.filter_w) * ct_idx] =
          matrix[row][col];
    }
  }

  vector<Plaintext> enc_mat(data.inp_ct);
  for (uint64_t ct = 0; ct < data.inp_ct; ct++) {
    batch_encoder.encode(mat_pack[ct], enc_mat[ct]);
  }
  return enc_mat;
}

/* Generates a masking vector of random noise that will be applied to parts of
 * the ciphertext that contain leakage */
Plaintext fc_preprocess_noise(const uint64_t *secret_share,
                               const FCMetadata &data,
                               BatchEncoder &batch_encoder) {
  // Sample randomness into vector
  vector<uint64_t> noise(data.slot_count, 0ULL);
  PRG prg;
  random_mod_p(prg, noise.data(), data.slot_count, prime_mod);

  // Puncture the vector with secret shares where an actual fc result value
  // lives
  #pragma omp parallel for num_threads(num_threads) schedule(static)
  for (uint64_t row = 0; row < data.filter_h; row++) {
    uint64_t curr_set = row / data.inp_ct;
    noise[(row % data.inp_ct) + next_pow2(data.image_size) * curr_set] =
        secret_share[row];
  }

  Plaintext enc_noise;
  batch_encoder.encode(noise, enc_noise);

  return enc_noise;
}

Ciphertext fc_online(Ciphertext &ct, vector<Plaintext> &enc_mat,
                     const FCMetadata &data, Evaluator &evaluator,
                     GaloisKeys &gal_keys, RelinKeys &relin_keys, Ciphertext &zero) {
  Ciphertext result = zero;
  // For each matrix ciphertext, rotate the input vector once and multiply + add
  Ciphertext tmp;
  for (uint64_t ct_idx = 0; ct_idx < data.inp_ct; ct_idx++) {
      evaluator.rotate_rows(ct, ct_idx, gal_keys, tmp);
      evaluator.multiply_plain_inplace(tmp, enc_mat[ct_idx]);
      evaluator.relinearize_inplace(tmp, relin_keys);
      evaluator.add_inplace(result, tmp);
  }

  // Rotate all partial sums together
  for (uint64_t rot = data.inp_ct; rot < next_pow2(data.image_size); rot *= 2) {
    Ciphertext tmp;
    if (rot == data.slot_count / 2) {
      evaluator.rotate_columns(result, gal_keys, tmp);
    } else {
      evaluator.rotate_rows(result, rot, gal_keys, tmp);
    }
    evaluator.add_inplace(result, tmp);
  }

  return result;
}

Ciphertext fc_online_1(Ciphertext &ct, vector<Plaintext> &enc_mat,
                     const FCMetadata &data, Evaluator &evaluator,
                     GaloisKeys &gal_keys, RelinKeys &relin_keys, Ciphertext &zero) {
  // For each matrix ciphertext, rotate the input vector once and multiply + add
  Ciphertext result;
  evaluator.multiply_plain(ct, enc_mat[0], result);
  evaluator.relinearize_inplace(result, relin_keys);

  return result;
}

uint64_t *fc_postprocess(Ciphertext &ct, const FCMetadata &data,
                         BatchEncoder &batch_encoder, Decryptor &decryptor) {
  vector<uint64_t> plain(data.slot_count, 0ULL);
  Plaintext tmp;
  decryptor.decrypt(ct, tmp);
  batch_encoder.decode(tmp, plain);

  uint64_t *result = new uint64_t[data.filter_h];
  #pragma omp parallel for num_threads(num_threads) schedule(static)
  for (uint64_t row = 0; row < data.filter_h; row++) {
    uint64_t curr_set = row / data.inp_ct;
    result[row] =
        plain[(row % data.inp_ct) + next_pow2(data.image_size) * curr_set];
  }
  return result;
}

uint64_t *fc_postprocess_1(Ciphertext &ct, const FCMetadata &data,
                         BatchEncoder &batch_encoder, Decryptor &decryptor) {
  vector<uint64_t> plain(data.slot_count, 0ULL);
  Plaintext tmp;
  decryptor.decrypt(ct, tmp);
  batch_encoder.decode(tmp, plain);

  uint64_t *result = new uint64_t[data.filter_h];
  memset(result, 0, data.filter_h*sizeof(uint64_t));
  // #pragma omp parallel for num_threads(num_threads) schedule(static)
  for (uint64_t row = 0; row < data.filter_h; row++) {
    uint64_t curr_set = row / data.inp_ct;
    for(uint64_t col = 0; col < data.image_size; col++){
      result[row] = (result[row] + plain[col])%prime_mod;
    }
  }
  return result;
}

uint64_t *fc_postprocess_mac(Ciphertext &ct, const FCMetadata &data,
                         BatchEncoder &batch_encoder, Decryptor &decryptor) {
  vector<uint64_t> plain;
  Plaintext tmp;
  decryptor.decrypt(ct, tmp);
  batch_encoder.decode(tmp, plain);

  uint64_t *result = new uint64_t[data.image_size];
  #pragma omp parallel for num_threads(num_threads) schedule(static)
  for (uint64_t col = 0; col < data.image_size; col++) {
       result[col] = plain[col];
  }
  return result;
}

FCField::FCField() {
  this->num_threads = 1;
  this->ios = nullptr;
}

FCField::FCField(int party, NetIO *io) {
  this->party = party;
  this->io = io;
  this->num_threads = 1;
  this->ios = nullptr;
  this->slot_count = POLY_MOD_DEGREE;
  generate_new_keys(party, io, slot_count, context, encryptor, decryptor,
                    evaluator, encoder, gal_keys, relin_keys, zero);
}

FCField::FCField(int party, emp::NetIO *io, emp::BoolIO<emp::NetIO> **ios, int num_threads){
  this->party = party;
  this->io = io;
  this->ios = ios;
  this->num_threads = num_threads;
  this->slot_count = POLY_MOD_DEGREE;
  generate_new_keys(party, io, slot_count, context, encryptor, decryptor,
                    evaluator, encoder, gal_keys, relin_keys, zero);
}

FCField::~FCField() {
  free_keys(party, encryptor, decryptor, evaluator, encoder, gal_keys, relin_keys, zero);
}

void FCField::init(int party, emp::NetIO *io){
  this->party = party;
  this->io = io;
  this->num_threads = 1;
  this->ios = nullptr;
  this->slot_count = POLY_MOD_DEGREE;
  generate_new_keys(party, io, slot_count, context, encryptor, decryptor,
                    evaluator, encoder, gal_keys, relin_keys, zero);
}

void FCField::init(int party, emp::NetIO *io, emp::BoolIO<emp::NetIO> **ios, int num_threads){
  this->party = party;
  this->io = io;
  this->ios = ios;
  this->num_threads = num_threads;
  this->slot_count = POLY_MOD_DEGREE;
  generate_new_keys(party, io, slot_count, context, encryptor, decryptor,
                    evaluator, encoder, gal_keys, relin_keys, zero);
}

void FCField::configure() {
  data.slot_count = slot_count;
  // Only works with a ciphertext that fits in a single ciphertext
  assert(data.slot_count >= data.image_size);

  data.filter_size = data.filter_h * data.filter_w;
  // How many columns of matrix we can fit in a single ciphertext
  data.pack_num = slot_count / next_pow2(data.filter_w);
  // How many total ciphertexts we'll need
  data.inp_ct = ceil((float)next_pow2(data.filter_h) / data.pack_num);
  data.inp_ct_1 = ceil((float)next_pow2(data.filter_w) / data.pack_num);
}

void FCField::configure_1() {
  data_1.slot_count = slot_count;
  // Only works with a ciphertext that fits in a single ciphertext
  assert(data_1.slot_count >= data_1.image_size);

  data_1.filter_size = data_1.filter_h * data_1.filter_w;
  // How many columns of matrix we can fit in a single ciphertext
  data_1.pack_num = slot_count / next_pow2(data_1.filter_w);
  // How many total ciphertexts we'll need
  data_1.inp_ct = ceil((float)next_pow2(data_1.filter_h) / data_1.pack_num);
  data_1.inp_ct_1 = ceil((float)next_pow2(data_1.filter_w) / data_1.pack_num);
}

vector<uint64_t> FCField::ideal_functionality(uint64_t *vec, uint64_t **matrix, seal::Modulus mod) {
  vector<uint64_t> result(data.filter_h, 0ULL);
  for (uint64_t row = 0; row < data.filter_h; row++) {
    for (uint64_t idx = 0; idx < data.filter_w; idx++) {
      uint64_t partial = mod_mult(vec[idx], matrix[row][idx], mod);
      result[row] = (result[row] + partial)%prime_mod;
    }
  }
  return result;
}

vector<uint64_t> FCField::ideal_functionality(uint64_t *vec, uint64_t **matrix, uint64_t rows, uint64_t common_dim, seal::Modulus mod) {
  vector<uint64_t> result(rows, 0ULL);
  for (uint64_t row = 0; row < rows; row++) {
    for (uint64_t idx = 0; idx < common_dim; idx++) {
      uint64_t partial = mod_mult(vec[idx], matrix[row][idx], mod);
      result[row] = (result[row] + partial)%prime_mod;
    }
  }
  return result;
}

void FCField::matrix_multiplication_first(int32_t num_rows, int32_t common_dim,
                                    int32_t num_cols,
                                    vector<vector<uint64_t>> &inputs,
                                    vector<vector<uint64_t>> &op_shares,
                                    vector<vector<uint64_t>> &mac_op_shares,
                                    seal::Modulus mod,
                                    bool verify_output, bool verbose) {

  assert(num_cols == 1);
  data.filter_h = num_rows;
  data.filter_w = common_dim;
  data.image_size = common_dim;
  this->slot_count =
      min(max(8192, 2 * next_pow2(common_dim)), SEAL_POLY_MOD_DEGREE_MAX);
  configure();

  Encryptor *encryptor_;
  Decryptor *decryptor_;
  Evaluator *evaluator_;
  BatchEncoder *encoder_;
  GaloisKeys *gal_keys_;
  RelinKeys *relin_keys_;
  Ciphertext *zero_;
  if (slot_count > POLY_MOD_DEGREE) {
    generate_new_keys(party, io, slot_count, context, encryptor_, decryptor_,
                      evaluator_, encoder_, gal_keys_, relin_keys_, zero_);
  } else {
    encryptor_ = this->encryptor;
    decryptor_ = this->decryptor;
    evaluator_ = this->evaluator;
    encoder_ = this->encoder;
    gal_keys_ = this->gal_keys;
    relin_keys_ = this->relin_keys;
    zero_ = this->zero;
  }

  if(party == BOB) {
    vector<uint64_t> vec(common_dim);
    for (uint64_t i = 0; i < common_dim; i++) {
      vec[i] = inputs[i][0];
    }

    if (verbose)
      cout << "[Client] Vector Generated" << endl;

    auto ct = preprocess_vec(vec.data(), data, *encryptor_, *encoder_);
    send_ciphertext(io, ct);
    if (verbose)
      cout << "[Client] Vector processed and sent" << endl;

    Ciphertext linear;
    Ciphertext linear_mac;
    recv_ciphertext(io, context, linear);
    recv_ciphertext(io, context, linear_mac);
    if (verbose)
      cout << "[Client] Receive ciphertexts of shares" << endl;

    auto linear_1 = fc_postprocess(linear, data, *encoder_, *decryptor_);
    auto linear_mac_1 = fc_postprocess(linear_mac, data, *encoder_, *decryptor_);

    if (verbose)
      cout << "[Client] Obtain shares" << endl;

    if(verify_output) {
      io->send_data(vec.data(), sizeof(uint64_t) * common_dim);
      io->send_data(linear_1, sizeof(uint64_t) * num_rows);
      io->send_data(linear_mac_1, sizeof(uint64_t) * num_rows);
    }
  } else {
    PRG prg;
    //Generate Random MAC
    uint64_t mac_key;
    random_mod_p(prg, &mac_key, 1, prime_mod);
    vector<uint64_t> mac_vec(encoder_->slot_count(), mac_key);
    Plaintext* enc_mac = new Plaintext();
    encoder_->encode(mac_vec, *enc_mac);

    //Generate random_shares
    vector<uint64_t> op_shares_vec(num_rows, 0);
    vector<uint64_t> mac_op_shares_vec(num_rows, 0);
    random_mod_p(prg, op_shares_vec.data(), num_rows, prime_mod);
    random_mod_p(prg, mac_op_shares_vec.data(), num_rows, prime_mod);

    Plaintext linear_0 = fc_preprocess_noise(op_shares_vec.data(), data, *encoder_);
    Plaintext linear_mac_0 = fc_preprocess_noise(mac_op_shares_vec.data(), data, *encoder_);
    if (verbose)
      cout << "[Server] Generate shares" << endl;

    //Preprocess Matrix
    vector<uint64_t *> matrix(num_rows);
    for (uint64_t i = 0; i < num_rows; i++) {
      matrix[i] = new uint64_t[common_dim];
      for (uint64_t j = 0; j < common_dim; j++) {
        matrix[i][j] = inputs[i][j];
      }
    }
    auto encoded_mat = preprocess_matrix(matrix.data(), data, *encoder_);
    if (verbose)
      cout << "[Server] Preprocess Matrix" << endl;


    //Receive ciphertext from client
    Ciphertext ct;
    recv_ciphertext(io, context, ct);
    if (verbose)
      cout << "[Server] Receive Ciphertext from client" << endl;

    //Compute FC component
    Ciphertext linear = fc_online(ct, encoded_mat, data, *evaluator_, *gal_keys_, *relin_keys_,
                               *zero_);
    if (verbose)
      cout << "[Server] Compute FC" << endl;

    Ciphertext linear_mac;
    evaluator_->multiply_plain(linear, *enc_mac, linear_mac);
    //Linear Share
    evaluator_->sub_plain_inplace(linear, linear_0);
    //Linear MAC Share
    evaluator_->sub_plain_inplace(linear_mac, linear_mac_0);
    if (verbose)
      cout << "[Server] Generate Client shares" << endl;

    //Send ciphertexts to client
    send_ciphertext(io, linear);
    send_ciphertext(io, linear_mac);
    if (verbose)
      cout << "[Server] Send ciphertexts of shares to client" << endl;

    //Verify
    if(verify_output) {
      vector<uint64_t>  vec(common_dim);
      //receive client input
      io->recv_data(vec.data(), sizeof(uint64_t)*common_dim);
      //Compute FC
      auto result_actual = ideal_functionality(vec.data(), matrix.data(), mod);
      //Compute MAC
      vector<uint64_t> result_actual_mac(num_rows);

      for(uint64_t i=0;i<num_rows; i++) {
        result_actual_mac[i] = mod_mult(mac_key,result_actual[i], mod);
      }

      //receive client_shares
      vector<uint64_t> op_shares_vec_1(num_rows);
      vector<uint64_t> mac_op_shares_vec_1(num_rows);
      io->recv_data(op_shares_vec_1.data(), sizeof(uint64_t)*num_rows);
      io->recv_data(mac_op_shares_vec_1.data(), sizeof(uint64_t)*num_rows);
      // reconstruct output
      for(uint64_t i=0; i<num_rows; i++) {
        op_shares_vec_1[i] = (op_shares_vec[i] + op_shares_vec_1[i])%prime_mod;
        mac_op_shares_vec_1[i] = (mac_op_shares_vec[i] + mac_op_shares_vec_1[i])%prime_mod;
      }

      //check for equality
      uint64_t ctr=0;
      uint64_t mctr=0;
      for(uint64_t i=0;i<num_rows; i++) {
        if(result_actual[i] == op_shares_vec_1[i])
          ctr++;
        if(result_actual_mac[i] == mac_op_shares_vec_1[i])
          mctr++;
      }
      cout<<"Correct Shares: "<< ctr<<endl;
      cout<<"Correct Mac Shares: "<<mctr<<endl;
    }
  }
}

void FCField::matrix_multiplication_gen(int32_t num_rows, int32_t common_dim,
                                    int32_t num_cols,
                                    vector<vector<uint64_t>> &matrix,
                                    vector<vector<uint64_t>> &input_share,
                                    vector<vector<uint64_t>> &mac_input_share,
                                    seal::Modulus mod,
                                    bool verify_output, bool verbose) {

  assert(num_cols == 1);
  data.filter_h = num_rows;
  data.filter_w = common_dim;
  data.image_size = common_dim;
  this->slot_count =
      min(max(8192, 2 * next_pow2(common_dim)), SEAL_POLY_MOD_DEGREE_MAX);
  configure();

  Encryptor *encryptor_;
  Decryptor *decryptor_;
  Evaluator *evaluator_;
  BatchEncoder *encoder_;
  GaloisKeys *gal_keys_;
  RelinKeys *relin_keys_;
  Ciphertext *zero_;
  if (slot_count > POLY_MOD_DEGREE) {
    generate_new_keys(party, io, slot_count, context, encryptor_, decryptor_,
                      evaluator_, encoder_, gal_keys_, relin_keys_, zero_);
  } else {
    encryptor_ = this->encryptor;
    decryptor_ = this->decryptor;
    evaluator_ = this->evaluator;
    encoder_ = this->encoder;
    gal_keys_ = this->gal_keys;
    relin_keys_ = this->relin_keys;
    zero_ = this->zero;
  }

  if(party == BOB) {
    vector<uint64_t> input_vec(common_dim);
    vector<uint64_t> mac_input_vec(common_dim);
    for (uint64_t i = 0; i < common_dim; i++) {
      input_vec[i] = input_share[i][0];
      mac_input_vec[i] = mac_input_share[i][0];
    }
    if (verbose)
      cout << "[Client] Vector Generated" << endl;

    auto input_ct = preprocess_vec(input_vec.data(), data, *encryptor_, *encoder_);
    auto mac_input_ct = preprocess_vec(mac_input_vec.data(), data, *encryptor_, *encoder_);
    send_ciphertext(io, input_ct);
    send_ciphertext(io, mac_input_ct);

    if (verbose)
      cout << "[Client] Vector processed and sent" << endl;

    Ciphertext linear;
    Ciphertext linear_mac;
    Ciphertext mac_ver_ip;
    recv_ciphertext(io, context, linear);
    recv_ciphertext(io, context, linear_mac);
    recv_ciphertext(io, context, mac_ver_ip);
    if (verbose)
      cout << "[Client] Receive ciphertexts of shares" << endl;

    auto linear_1 = fc_postprocess(linear, data, *encoder_, *decryptor_);
    auto linear_mac_1 = fc_postprocess(linear_mac, data, *encoder_, *decryptor_);
    auto mac_ver_ip_1 = fc_postprocess_mac(mac_ver_ip, data, *encoder_, *decryptor_);

    if (verbose)
      cout << "[Client] Obtain shares" << endl;

    if(verify_output) {
      io->send_data(input_vec.data(), sizeof(uint64_t) * common_dim);
      io->send_data(mac_input_vec.data(), sizeof(uint64_t) * common_dim);
      io->send_data(linear_1, sizeof(uint64_t) * num_rows);
      io->send_data(linear_mac_1, sizeof(uint64_t) * num_rows);
      io->send_data(mac_ver_ip_1, sizeof(uint64_t) * common_dim);
    }
  } else {
    vector<uint64_t> input_vec(common_dim);
    vector<uint64_t> mac_input_vec(common_dim);
    for (uint64_t i = 0; i < common_dim; i++) {
      input_vec[i] = input_share[i][0];
      mac_input_vec[i] = mac_input_share[i][0];
    }

    Plaintext input_pt = preprocess_vec_plain(input_vec.data(), data, *encoder_);
    Plaintext mac_input_pt = preprocess_vec_plain(mac_input_vec.data(), data, *encoder_);

    PRG prg;
    //Generate Random MAC
    uint64_t mac_key;
    random_mod_p(prg, &mac_key, 1, prime_mod);
    uint64_t mac_key_sqrt = mod_mult(mac_key, mac_key, mod);
    uint64_t mac_key_cube = mod_mult(mac_key_sqrt, mac_key, mod);

    vector<uint64_t> mac_vec_sqrt(encoder_->slot_count(), mac_key_sqrt);
    Plaintext* enc_mac_sqrt = new Plaintext();
    encoder_->encode(mac_vec_sqrt, *enc_mac_sqrt);

    vector<uint64_t> mac_vec_cube(encoder_->slot_count(), mac_key_cube);
    Plaintext* enc_mac_cube = new Plaintext();
    encoder_->encode(mac_vec_cube, *enc_mac_cube);

    //Generate random_shares
    vector<uint64_t> op_shares_vec(num_rows, 0);
    vector<uint64_t> mac_op_shares_vec(num_rows, 0);
    vector<uint64_t> mac_ver_shares_vec(common_dim, 0);
    random_mod_p(prg, op_shares_vec.data(), num_rows, prime_mod);
    random_mod_p(prg, mac_op_shares_vec.data(), num_rows, prime_mod);
    random_mod_p(prg, mac_ver_shares_vec.data(), common_dim, prime_mod);

    Plaintext linear_0 = fc_preprocess_noise(op_shares_vec.data(), data, *encoder_);
    Plaintext linear_mac_0 = fc_preprocess_noise(mac_op_shares_vec.data(), data, *encoder_);
    Plaintext mac_ver_shares_0 = preprocess_vec_plain(mac_ver_shares_vec.data(), data, *encoder_);
    if (verbose)
      cout << "[Server] Generate shares" << endl;

    //Preprocess Matrix
    vector<uint64_t *> matrixd(num_rows);
    for (uint64_t i = 0; i < num_rows; i++) {
      matrixd[i] = new uint64_t[common_dim];
      for (uint64_t j = 0; j < common_dim; j++) {
        matrixd[i][j] = matrix[i][j];
      }
    }
    auto encoded_mat = preprocess_matrix(matrixd.data(), data, *encoder_);
    if (verbose)
      cout << "[Server] Preprocess Matrix" << endl;


    //Receive ciphertext from client
    Ciphertext input_ct;
    Ciphertext mac_input_ct;
    recv_ciphertext(io, context, input_ct);
    recv_ciphertext(io, context, mac_input_ct);
    if (verbose)
      cout << "[Server] Receive Ciphertext from client" << endl;

    //Add Shares
    evaluator_->add_plain_inplace(input_ct, input_pt);
    evaluator_->add_plain_inplace(mac_input_ct, mac_input_pt);

    //Compute FC component
    Ciphertext linear = fc_online(input_ct, encoded_mat, data, *evaluator_, *gal_keys_, *relin_keys_,
                               *zero_);
    Ciphertext linear_mac = fc_online(mac_input_ct, encoded_mat, data, *evaluator_, *gal_keys_, *relin_keys_,
                               *zero_);
    Ciphertext mac_ver_op_cube;
    Ciphertext mac_ver_op_sqrt;
    Ciphertext mac_ver_op;
    evaluator_->multiply_plain(input_ct, *enc_mac_cube, mac_ver_op_cube);
    evaluator_->multiply_plain(mac_input_ct, *enc_mac_sqrt, mac_ver_op_sqrt);
    evaluator_->sub(mac_ver_op_cube, mac_ver_op_sqrt, mac_ver_op);

    if (verbose)
      cout << "[Server] Compute FC" << endl;

    //Linear Share
    evaluator_->sub_plain_inplace(linear, linear_0);
    //Linear MAC Share
    evaluator_->sub_plain_inplace(linear_mac, linear_mac_0);
    //MAC Verification Share
    evaluator->sub_plain_inplace(mac_ver_op, mac_ver_shares_0);
    if (verbose)
      cout << "[Server] Generate Client shares" << endl;

    //Send ciphertexts to client
    send_ciphertext(io, linear);
    send_ciphertext(io, linear_mac);
    send_ciphertext(io, mac_ver_op);
    if (verbose)
      cout << "[Server] Send ciphertexts of shares to client" << endl;

    //Verify
    if(verify_output) {
      vector<uint64_t>  input_vec_1(common_dim);
      vector<uint64_t>  mac_input_vec_1(common_dim);
      //receive client input
      io->recv_data(input_vec_1.data(), sizeof(uint64_t)*common_dim);
      io->recv_data(mac_input_vec_1.data(), sizeof(uint64_t)*common_dim);
      for(uint64_t i=0; i<common_dim; i++) {
        input_vec_1[i] = (input_vec_1[i] + input_vec[i]) % prime_mod;
        mac_input_vec_1[i] = (mac_input_vec_1[i] + mac_input_vec[i]) % prime_mod;
      }

      //Compute FC
      auto result_actual_input = ideal_functionality(input_vec_1.data(), matrixd.data(), mod);
      auto result_actual_mac = ideal_functionality(mac_input_vec_1.data(), matrixd.data(), mod);

      vector<uint64_t> mac_ver_actual(common_dim, 0);
      for(uint64_t i=0; i<common_dim; i++) {
        mac_ver_actual[i] = (mod_mult(mac_key_cube, input_vec_1[i], mod) + (prime_mod - mod_mult(mac_key_sqrt, mac_input_vec_1[i], mod)))%prime_mod;
      }

      //receive client_shares
      vector<uint64_t> op_shares_vec_1(num_rows);
      vector<uint64_t> mac_op_shares_vec_1(num_rows);
      vector<uint64_t> mac_ver_shares_vec_1(common_dim);
      io->recv_data(op_shares_vec_1.data(), sizeof(uint64_t)*num_rows);
      io->recv_data(mac_op_shares_vec_1.data(), sizeof(uint64_t)*num_rows);
      io->recv_data(mac_ver_shares_vec_1.data(), sizeof(uint64_t)*common_dim);
      // reconstruct output
      for(uint64_t i=0; i<num_rows; i++) {
        op_shares_vec_1[i] = (op_shares_vec[i] + op_shares_vec_1[i])%prime_mod;
        mac_op_shares_vec_1[i] = (mac_op_shares_vec[i] + mac_op_shares_vec_1[i])%prime_mod;
      }

      for(uint64_t i=0; i<common_dim; i++) {
        mac_ver_shares_vec_1[i] = (mac_ver_shares_vec[i] + mac_ver_shares_vec_1[i])%prime_mod;
      }

      //check for equality
      uint64_t ctr=0;
      uint64_t mctr=0;
      uint64_t mvctr=0;
      for(uint64_t i=0;i<num_rows; i++) {
        if(result_actual_input[i] == op_shares_vec_1[i])
          ctr++;
        if(result_actual_mac[i] == mac_op_shares_vec_1[i])
          mctr++;
      }
      for(uint64_t i=0; i<common_dim; i++) {
        if(mac_ver_actual[i] == mac_ver_shares_vec_1[i]) {
            mvctr++;
        }
      }
      cout<<"Correct Shares: "<< ctr<<endl;
      cout<<"Correct Mac Shares: "<<mctr<<endl;
      cout<<"Correct Mac Verification Shares: "<<mvctr<<endl;
    }
  }
}

void FCField::vector_multiplication(uint64_t num_rows, uint64_t common_dim,
                                    uint64_t num_cols,
                                    vector<vector<uint64_t>> &matrix,
                                    vector<vector<uint64_t>> &input_share,
                                    vector<vector<uint64_t>> &mac_input_share,
                                    uint64_t mac_key,
                                    seal::Modulus mod,
                                    bool verify_output, bool verbose) {

  assert((num_cols == 1) && (num_rows == 1));
  data.filter_h = num_rows;
  data.filter_w = common_dim;
  data.image_size = common_dim;

  this->slot_count = min(max(POLY_MOD_DEGREE, 2 * next_pow2(common_dim)), (uint64_t)SEAL_POLY_MOD_DEGREE_MAX);
  configure();

  Encryptor *encryptor_;
  Decryptor *decryptor_;
  Evaluator *evaluator_;
  BatchEncoder *encoder_;
  GaloisKeys *gal_keys_;
  RelinKeys *relin_keys_;
  Ciphertext *zero_;
  if (slot_count > POLY_MOD_DEGREE) {
    generate_new_keys(party, io, slot_count, context, encryptor_, decryptor_,
                      evaluator_, encoder_, gal_keys_, relin_keys_, zero_);
  } else {
    encryptor_ = this->encryptor;
    decryptor_ = this->decryptor;
    evaluator_ = this->evaluator;
    encoder_ = this->encoder;
    gal_keys_ = this->gal_keys;
    relin_keys_ = this->relin_keys;
    zero_ = this->zero;
  }

  if(party == BOB) {
    vector<uint64_t> input_vec(common_dim);
    vector<uint64_t> mac_input_vec(common_dim);
    for (uint64_t i = 0; i < common_dim; i++) {
      input_vec[i] = input_share[i][0];
      mac_input_vec[i] = mac_input_share[i][0];
    }
    if (verbose)
      cout << "[Client] Vector Generated" << endl;

    auto input_ct = preprocess_vec(input_vec.data(), data, *encryptor_, *encoder_);
    // auto mac_input_ct = preprocess_vec(mac_input_vec.data(), data, *encryptor_, *encoder_);
    send_ciphertext(io, input_ct);
    // send_ciphertext(io, mac_input_ct);

    if (verbose)
      cout << "[Client] Vector processed and sent" << endl;

    Ciphertext linear;
    Ciphertext linear_mac;
    Ciphertext mac_ver_ip;
    recv_ciphertext(io, context, linear);
    recv_ciphertext(io, context, linear_mac);
    recv_ciphertext(io, context, mac_ver_ip);
    if (verbose)
      cout << "[Client] Receive ciphertexts of shares" << endl;

    auto linear_1 = fc_postprocess(linear, data, *encoder_, *decryptor_);
    auto linear_mac_1 = fc_postprocess(linear_mac, data, *encoder_, *decryptor_);
    auto mac_ver_ip_1 = fc_postprocess_mac(mac_ver_ip, data, *encoder_, *decryptor_);
    // std::cout<<"linear:"<<(linear_mac_1[0]+(prime_mod - mod_mult(mac_key, linear_1[0], mod)))%prime_mod<<std::endl;

    if (verbose)
      cout << "[Client] Obtain shares" << endl;

    // Verify
    vector<uint64_t> verify_msg(common_dim);
    for (uint64_t i = 0; i < common_dim; i++) {
      verify_msg[i] = (mac_input_vec[i] + (prime_mod - mac_ver_ip_1[i])) % prime_mod;
    }
    if (verbose)
      cout << "[Client] Verify message Generated" << endl;
    io->send_data(verify_msg.data(), sizeof(uint64_t) * common_dim);
    if(verify_output) {
      io->send_data(input_vec.data(), sizeof(uint64_t) * common_dim);
      io->send_data(mac_input_vec.data(), sizeof(uint64_t) * common_dim);
      io->send_data(linear_1, sizeof(uint64_t) * num_rows);
      io->send_data(linear_mac_1, sizeof(uint64_t) * num_rows);
    }
  } else {
    vector<uint64_t> input_vec(common_dim);
    vector<uint64_t> mac_input_vec(common_dim);
    for (uint64_t i = 0; i < common_dim; i++) {
      input_vec[i] = input_share[i][0];
      mac_input_vec[i] = mac_input_share[i][0];
    }

    Plaintext input_pt = preprocess_vec_plain(input_vec.data(), data, *encoder_);
    Plaintext mac_input_pt = preprocess_vec_plain(mac_input_vec.data(), data, *encoder_);

    PRG prg;
    //Generate Random MAC
    // uint64_t mac_key;
    // random_mod_p(prg, &mac_key, 1, prime_mod);
    vector<uint64_t> mac_vec(encoder_->slot_count(), mac_key);
    Plaintext* enc_mac = new Plaintext();
    encoder_->encode(mac_vec, *enc_mac);
    // uint64_t mac_key_sqrt = mod_mult(mac_key, mac_key, mod);
    // uint64_t mac_key_cube = mod_mult(mac_key_sqrt, mac_key, mod);

    // vector<uint64_t> mac_vec_sqrt(encoder_->slot_count(), mac_key_sqrt);
    // Plaintext* enc_mac_sqrt = new Plaintext();
    // encoder_->encode(mac_vec_sqrt, *enc_mac_sqrt);

    // vector<uint64_t> mac_vec_cube(encoder_->slot_count(), mac_key_cube);
    // Plaintext* enc_mac_cube = new Plaintext();
    // encoder_->encode(mac_vec_cube, *enc_mac_cube);

    //Generate random_shares
    vector<uint64_t> op_shares_vec(num_rows, 0);
    vector<uint64_t> mac_op_shares_vec(num_rows, 0);
    vector<uint64_t> mac_ver_shares_vec(common_dim, 0);
    random_mod_p(prg, op_shares_vec.data(), num_rows, prime_mod);
    random_mod_p(prg, mac_op_shares_vec.data(), num_rows, prime_mod);
    random_mod_p(prg, mac_ver_shares_vec.data(), common_dim, prime_mod);

    Plaintext linear_0 = fc_preprocess_noise(op_shares_vec.data(), data, *encoder_);
    Plaintext linear_mac_0 = fc_preprocess_noise(mac_op_shares_vec.data(), data, *encoder_);
    Plaintext mac_ver_shares_0 = preprocess_vec_plain(mac_ver_shares_vec.data(), data, *encoder_);
    if (verbose)
      cout << "[Server] Generate shares" << endl;

    //Preprocess Matrix
    vector<uint64_t *> matrixd(num_rows);
    vector<uint64_t *> mac_matrixd(num_rows);
    for (uint64_t i = 0; i < num_rows; i++) {
      matrixd[i] = new uint64_t[common_dim];
      mac_matrixd[i] = new uint64_t[common_dim];
      for (uint64_t j = 0; j < common_dim; j++) {
        matrixd[i][j] = matrix[i][j];
        mac_matrixd[i][j] = mod_mult(mac_key, matrix[i][j], prime_mod);
      }
    }
    auto encoded_mat = preprocess_matrix(matrixd.data(), data, *encoder_);
    auto encoded_mac_mat = preprocess_matrix(mac_matrixd.data(), data, *encoder_);
    if (verbose)
      cout << "[Server] Preprocess Matrix" << endl;


    //Receive ciphertext from client
    Ciphertext input_ct_0;
    // Ciphertext mac_input_ct;
    recv_ciphertext(io, context, input_ct_0);
    // recv_ciphertext(io, context, mac_input_ct);
    if (verbose)
      cout << "[Server] Receive Ciphertext from client" << endl;

    //Add Shares
    Ciphertext input_ct;
    evaluator_->add_plain(input_ct_0, input_pt, input_ct);
    // evaluator_->add_plain_inplace(mac_input_ct, mac_input_pt);

    //Compute FC component
    Ciphertext linear = fc_online(input_ct, encoded_mat, data, *evaluator_, *gal_keys_, *relin_keys_, *zero_);
    // Ciphertext linear_mac = fc_online(mac_input_ct, encoded_mat, data, *evaluator_, *gal_keys_, *relin_keys_, *zero_);
    Ciphertext linear_mac = fc_online(input_ct, encoded_mac_mat, data, *evaluator_, *gal_keys_, *relin_keys_, *zero_);

    // Ciphertext mac_ver_op_cube;
    // Ciphertext mac_ver_op_sqrt;
    // Ciphertext linear_mac;
    Ciphertext mac_ver_op;
    // evaluator_->multiply_plain(linear, *enc_mac, linear_mac);
    evaluator_->multiply_plain(input_ct_0, *enc_mac, mac_ver_op);


    if (verbose)
      cout << "[Server] Compute FC" << endl;

    //Linear Share
    evaluator_->sub_plain_inplace(linear, linear_0);
    //Linear MAC Share
    evaluator_->sub_plain_inplace(linear_mac, linear_mac_0);
    //MAC Verification Share
    evaluator->sub_plain_inplace(mac_ver_op, mac_ver_shares_0);

    if (verbose)
      cout << "[Server] Generate Client shares" << endl;

    //Send ciphertexts to client
    send_ciphertext(io, linear);
    send_ciphertext(io, linear_mac);
    send_ciphertext(io, mac_ver_op);
    if (verbose)
      cout << "[Server] Send ciphertexts of shares to client" << endl;

    // Verify
    vector<uint64_t> verify_msg(common_dim);
    io->recv_data(verify_msg.data(), sizeof(uint64_t)*common_dim);
    uint64_t ctr = 0;
    for (uint64_t i = 0; i < common_dim; i++){
      if((mod_mult(mac_key, input_vec[i], mod) + mac_ver_shares_vec[i] + (prime_mod - mac_input_vec[i])) % prime_mod == verify_msg[i])
        ctr++;
      // std::cout << "verify_msg[i]:" << verify_msg[i] << std::endl;
    }
    cout << "Correct input Shares: " << ctr << endl;
    if(ctr < common_dim) cout << "Abort!"  << endl;

    if(verify_output) {
      vector<uint64_t>  input_vec_1(common_dim);
      vector<uint64_t>  mac_input_vec_1(common_dim);
      //receive client input
      io->recv_data(input_vec_1.data(), sizeof(uint64_t)*common_dim);
      io->recv_data(mac_input_vec_1.data(), sizeof(uint64_t)*common_dim);
      for(uint64_t i=0; i<common_dim; i++) {
        input_vec_1[i] = (input_vec_1[i] + input_vec[i]) % prime_mod;
        mac_input_vec_1[i] = (mac_input_vec_1[i] + mac_input_vec[i]) % prime_mod;
      }

      //Compute FC
      auto result_actual_input = ideal_functionality(input_vec_1.data(), matrixd.data(), mod);
      auto result_actual_mac = ideal_functionality(mac_input_vec_1.data(), matrixd.data(), mod);

      // vector<uint64_t> mac_ver_actual(common_dim, 0);
      // for(uint64_t i=0; i<common_dim; i++) {
      //   mac_ver_actual[i] = (mod_mult(mac_key_cube, input_vec_1[i], mod) + (prime_mod - mod_mult(mac_key_sqrt, mac_input_vec_1[i], mod)))%prime_mod;
      // }

      //receive client_shares
      vector<uint64_t> op_shares_vec_1(num_rows);
      vector<uint64_t> mac_op_shares_vec_1(num_rows);
      io->recv_data(op_shares_vec_1.data(), sizeof(uint64_t)*num_rows);
      io->recv_data(mac_op_shares_vec_1.data(), sizeof(uint64_t)*num_rows);
      // reconstruct output
      for(uint64_t i=0; i<num_rows; i++) {
        op_shares_vec_1[i] = (op_shares_vec[i] + op_shares_vec_1[i])%prime_mod;
        mac_op_shares_vec_1[i] = (mac_op_shares_vec[i] + mac_op_shares_vec_1[i])%prime_mod;
      }

      //check for equality
      uint64_t mctr=0;
      uint64_t mvctr=0;
      for(uint64_t i=0;i<num_rows; i++) {
        if(result_actual_input[i] == op_shares_vec_1[i])
          mctr++;
        if(result_actual_mac[i] == mac_op_shares_vec_1[i])
          mvctr++;
      }
      cout<<"Correct Computation: "<< mctr <<endl;
      cout<<"Correct Mac Shares: "<< mvctr <<endl;
    }
  }
}

void FCField::vector_multiplication_large(uint64_t num_rows, uint64_t common_dim,
                                    uint64_t num_cols,
                                    vector<vector<uint64_t>> &matrix,
                                    vector<vector<uint64_t>> &input_share,
                                    vector<vector<uint64_t>> &mac_input_share,
                                    uint64_t mac_key,
                                    seal::Modulus mod,
                                    bool verify_output, bool verbose) {

  assert((num_cols == 1) && (num_rows == 1));
  bool ss_zero = false;
  // 用多个密文块进行加密（密文块数量为ct_num），每个块能够加密的明文的数量为slot_count（取POLY_MOD_DEGREE）
  // 最后一个块可能没有装满，元素个数可能少于POLY_MOD_DEGREE，用data记录最后一个块的情况，data_1记录其余块的情况
  uint64_t ct_num = ceil((double)common_dim / POLY_MOD_DEGREE);
  uint64_t remain = common_dim % POLY_MOD_DEGREE;
  if(remain != 0){
    data.filter_h = num_rows;
    data.filter_w = remain;
    data.image_size = remain;
  }else{
    remain = POLY_MOD_DEGREE;
    data.filter_h = num_rows;
    data.filter_w = POLY_MOD_DEGREE;
    data.image_size = POLY_MOD_DEGREE;
  }
  data_1.filter_h = num_rows;
  data_1.filter_w = POLY_MOD_DEGREE;
  data_1.image_size = POLY_MOD_DEGREE;
  
  this->slot_count = POLY_MOD_DEGREE;
      // min(max(POLY_MOD_DEGREE, 2 * next_pow2(common_dim)), SEAL_POLY_MOD_DEGREE_MAX);
  configure();
  configure_1();

  Encryptor *encryptor_;
  Decryptor *decryptor_;
  Evaluator *evaluator_;
  BatchEncoder *encoder_;
  GaloisKeys *gal_keys_;
  RelinKeys *relin_keys_;
  Ciphertext *zero_;
  if (slot_count > POLY_MOD_DEGREE) {
    generate_new_keys(party, io, slot_count, context, encryptor_, decryptor_,
                      evaluator_, encoder_, gal_keys_, relin_keys_, zero_);
  } else {
    encryptor_ = this->encryptor;
    decryptor_ = this->decryptor;
    evaluator_ = this->evaluator;
    encoder_ = this->encoder;
    gal_keys_ = this->gal_keys;
    relin_keys_ = this->relin_keys;
    zero_ = this->zero;
  }


  if(party == BOB) {
    // 二维数组存储，便于分块加密和计算
    vector<vector<uint64_t>> input_vec(ct_num, vector<uint64_t>(slot_count, 0));
    vector<vector<uint64_t>> mac_input_vec(ct_num, vector<uint64_t>(slot_count, 0));
    for (uint64_t i = 0; i < ct_num; i++) {
      if(i < ct_num-1){
        for(uint64_t j=0; j<slot_count; j++){
          input_vec[i][j] = input_share[i*slot_count+j][0];
          mac_input_vec[i][j] = mac_input_share[i*slot_count+j][0];
        }
      }else{
        input_vec[i].resize(remain);
        mac_input_vec[i].resize(remain);
        for(uint64_t j=0; j<remain; j++){
          input_vec[i][j] = input_share[i*slot_count+j][0];
          mac_input_vec[i][j] = mac_input_share[i*slot_count+j][0];
        }
      }
    }
    if (verbose)
      std::cout << "[Client] Vector Generated" << std::endl;

    // 加密并发送给server
    vector<Ciphertext> input_ct(ct_num);
    for(uint64_t i=0; i<ct_num; i++){
      if(i<ct_num-1){
        input_ct[i] = preprocess_vec(input_vec[i].data(), data_1, *encryptor_, *encoder_);
      }else{
        input_ct[i] = preprocess_vec(input_vec[i].data(), data, *encryptor_, *encoder_);
      }
    }
    send_encrypted_vector(io, input_ct);
    // auto mac_input_ct = preprocess_vec(mac_input_vec.data(), data, *encryptor_, *encoder_);
    // send_ciphertext(io, input_ct);
    // send_ciphertext(io, mac_input_ct);

    if (verbose)
      std::cout << "[Client] Vector processed and sent" << std::endl;

    // 接收到server计算的输出并解密
    vector<Ciphertext> linear(ct_num);
    vector<Ciphertext> linear_mac(ct_num);
    vector<Ciphertext> mac_ver_ip(ct_num);
    recv_encrypted_vector(io, context, linear);
    recv_encrypted_vector(io, context, linear_mac);
    recv_encrypted_vector(io, context, mac_ver_ip);
    if (verbose)
      std::cout << "[Client] Receive ciphertexts of shares" << std::endl;
    // 解密函数返回的是地址
    vector<uint64_t*> linear_1(ct_num);
    vector<uint64_t*> linear_mac_1(ct_num);
    vector<uint64_t*> mac_ver_ip_1_addr(ct_num);
    for(uint64_t i=0; i<ct_num; i++){
      if(i<ct_num-1){
        linear_1[i] = fc_postprocess(linear[i], data_1, *encoder_, *decryptor_);
        linear_mac_1[i] = fc_postprocess(linear_mac[i], data_1, *encoder_, *decryptor_);
        mac_ver_ip_1_addr[i] = fc_postprocess_mac(mac_ver_ip[i], data_1, *encoder_, *decryptor_);
      }else{
        linear_1[i] = fc_postprocess(linear[i], data, *encoder_, *decryptor_);
        linear_mac_1[i] = fc_postprocess(linear_mac[i], data, *encoder_, *decryptor_);
        mac_ver_ip_1_addr[i] = fc_postprocess_mac(mac_ver_ip[i], data, *encoder_, *decryptor_);
      }
    }
    if (verbose)
      std::cout << "[Client] Obtain shares" << std::endl;

    // 求和得到向量内积、内积的mac、用于验证的message（注意向量内积、内积的mac都是一个数，而用于验证的message维度与输入相同）
    uint64_t sum_linear_1 = 0;
    uint64_t sum_linear_mac_1 = 0;
    vector<vector<uint64_t>> mac_ver_ip_1(ct_num, vector<uint64_t>(slot_count, 0));
    for(uint64_t i=0; i<ct_num; i++){
      sum_linear_1 = (sum_linear_1+linear_1[i][0])%prime_mod;
      sum_linear_mac_1 = (sum_linear_mac_1+linear_mac_1[i][0])%prime_mod;
      if(i<ct_num-1){
        for(uint64_t j=0; j<slot_count; j++){
          mac_ver_ip_1[i][j] = mac_ver_ip_1_addr[i][j];
        }
      }else{
        mac_ver_ip_1[i].resize(remain);
        for(uint64_t j=0; j<remain; j++){
          mac_ver_ip_1[i][j] = mac_ver_ip_1_addr[i][j];
        }
      }
    }

    // Verify：输入没有被篡改
    vector<uint64_t> verify_msg(common_dim);
    for(uint64_t i=0; i<ct_num; i++){
      if(i<ct_num-1){
        for(uint64_t j=0; j<slot_count; j++){
          verify_msg[i*slot_count+j] = (mac_input_vec[i][j] + (prime_mod - mac_ver_ip_1[i][j])) % prime_mod;
        }
      }else{
        for(uint64_t j=0; j<remain; j++){
          verify_msg[i*slot_count+j] = (mac_input_vec[i][j] + (prime_mod - mac_ver_ip_1[i][j])) % prime_mod;
        }
      }
    }
    if (verbose)
      std::cout << "[Client] Verify message Generated" << std::endl;
    io->send_data(verify_msg.data(), sizeof(uint64_t) * common_dim);
    
    if(verify_output) { // 结果正确性校验
      // 一维数组存储在本地，便于遍历
      vector<uint64_t> input_vec_local(common_dim);
      vector<uint64_t> mac_input_vec_local(common_dim);
      for (uint64_t i = 0; i < common_dim; i++) {
        input_vec_local[i] = input_share[i][0];
        mac_input_vec_local[i] = mac_input_share[i][0];
      }

      io->send_data(input_vec_local.data(), sizeof(uint64_t) * common_dim);
      io->send_data(mac_input_vec_local.data(), sizeof(uint64_t) * common_dim);
      io->send_data(&sum_linear_1, sizeof(uint64_t) * num_rows);
      io->send_data(&sum_linear_mac_1, sizeof(uint64_t) * num_rows);
    }
  } else {
    vector<uint64_t> input_vec_local(common_dim);
    vector<uint64_t> mac_input_vec_local(common_dim);
    for (uint64_t i = 0; i < common_dim; i++) {
      input_vec_local[i] = input_share[i][0];
      mac_input_vec_local[i] = mac_input_share[i][0];
    }

    vector<vector<uint64_t>> input_vec(ct_num, vector<uint64_t>(slot_count, 0));
    vector<vector<uint64_t>> mac_input_vec(ct_num, vector<uint64_t>(slot_count, 0));
    for (uint64_t i = 0; i < ct_num; i++) {
      if(i < ct_num-1){
        for(uint64_t j=0; j<slot_count; j++){
          input_vec[i][j] = input_vec_local[i*slot_count+j];
          mac_input_vec[i][j] = mac_input_vec_local[i*slot_count+j];
        }
      }else{
        input_vec[i].resize(remain);
        mac_input_vec[i].resize(remain);
        for(uint64_t j=0; j<remain; j++){
          input_vec[i][j] = input_vec_local[i*slot_count+j];
          mac_input_vec[i][j] = mac_input_vec_local[i*slot_count+j];
        }
      }
    }

    vector<Plaintext> input_pt(ct_num);
    vector<Plaintext> mac_input_pt(ct_num);
    for(uint64_t i=0; i<ct_num; i++){
      if(i<ct_num-1) {
        input_pt[i] = preprocess_vec_plain(input_vec[i].data(), data_1, *encoder_);
        mac_input_pt[i] = preprocess_vec_plain(mac_input_vec[i].data(), data_1, *encoder_);
      }else{
        input_pt[i] = preprocess_vec_plain(input_vec[i].data(), data, *encoder_);
        mac_input_pt[i] = preprocess_vec_plain(mac_input_vec[i].data(), data, *encoder_);
      }
    }

    PRG prg;
    //Generate Random MAC
    // uint64_t mac_key;
    // random_mod_p(prg, &mac_key, 1, prime_mod);
    vector<uint64_t> mac_vec(encoder_->slot_count(), mac_key);
    Plaintext* enc_mac = new Plaintext();
    encoder_->encode(mac_vec, *enc_mac);
    // uint64_t mac_key_sqrt = mod_mult(mac_key, mac_key, mod);
    // uint64_t mac_key_cube = mod_mult(mac_key_sqrt, mac_key, mod);

    // vector<uint64_t> mac_vec_sqrt(encoder_->slot_count(), mac_key_sqrt);
    // Plaintext* enc_mac_sqrt = new Plaintext();
    // encoder_->encode(mac_vec_sqrt, *enc_mac_sqrt);

    // vector<uint64_t> mac_vec_cube(encoder_->slot_count(), mac_key_cube);
    // Plaintext* enc_mac_cube = new Plaintext();
    // encoder_->encode(mac_vec_cube, *enc_mac_cube);

    //Generate random_shares
    vector<vector<uint64_t>> op_shares_vec(ct_num, vector<uint64_t>(num_rows, 0));
    vector<vector<uint64_t>> mac_op_shares_vec(ct_num, vector<uint64_t>(num_rows, 0));
    vector<vector<uint64_t>> mac_ver_shares_vec(ct_num, vector<uint64_t>(slot_count, 0));
    vector<Plaintext> linear_0(ct_num);
    vector<Plaintext> linear_mac_0(ct_num);
    vector<Plaintext> mac_ver_shares_0(ct_num);
    for(uint64_t i=0; i<ct_num; i++){
      if(!ss_zero){
        random_mod_p(prg, op_shares_vec[i].data(), num_rows, prime_mod);
        random_mod_p(prg, mac_op_shares_vec[i].data(), num_rows, prime_mod);
      }

      if(i<ct_num-1){
        if(!ss_zero) random_mod_p(prg, mac_ver_shares_vec[i].data(), slot_count, prime_mod);
        linear_0[i] = fc_preprocess_noise(op_shares_vec[i].data(), data_1, *encoder_);
        linear_mac_0[i] = fc_preprocess_noise(mac_op_shares_vec[i].data(), data_1, *encoder_);
        mac_ver_shares_0[i] = preprocess_vec_plain(mac_ver_shares_vec[i].data(), data_1, *encoder_);
      }else{
        mac_ver_shares_vec[i].resize(remain);
        if(!ss_zero) random_mod_p(prg, mac_ver_shares_vec[i].data(), remain, prime_mod);
        linear_0[i] = fc_preprocess_noise(op_shares_vec[i].data(), data, *encoder_);
        linear_mac_0[i] = fc_preprocess_noise(mac_op_shares_vec[i].data(), data, *encoder_);
        mac_ver_shares_0[i] = preprocess_vec_plain(mac_ver_shares_vec[i].data(), data, *encoder_);
      }
    }
    
    if (verbose)
      cout << "[Server] Generate shares" << endl;

    //Preprocess Matrix
    vector<vector<uint64_t *>> matrixd(ct_num, vector<uint64_t *>(num_rows));
    vector<vector<uint64_t *>> mac_matrixd(ct_num, vector<uint64_t *>(num_rows));
    vector<vector<Plaintext>> encoded_mat(ct_num);
    vector<vector<Plaintext>> encoded_mac_mat(ct_num);
    for(uint64_t i=0; i<ct_num; i++){
      if(i<ct_num-1){
        for (uint64_t j = 0; j < num_rows; j++) {
          matrixd[i][j] = new uint64_t[slot_count];
          mac_matrixd[i][j] = new uint64_t[slot_count];
          for (uint64_t k = 0; k < slot_count; k++) {
            matrixd[i][j][k] = matrix[j][i*slot_count+k];
            mac_matrixd[i][j][k] = mod_mult(mac_key, matrix[j][i*slot_count+k], prime_mod);
          }
        }
        encoded_mat[i] = preprocess_matrix(matrixd[i].data(), data_1, *encoder_);
        encoded_mac_mat[i] = preprocess_matrix(mac_matrixd[i].data(), data_1, *encoder_);
      }else{
        for (uint64_t j = 0; j < num_rows; j++) {
          matrixd[i][j] = new uint64_t[remain];
          mac_matrixd[i][j] = new uint64_t[remain];
          for (uint64_t k = 0; k < remain; k++) {
            matrixd[i][j][k] = matrix[j][i*slot_count+k];
            mac_matrixd[i][j][k] = mod_mult(mac_key, matrix[j][i*slot_count+k], prime_mod);
          }
        }
        encoded_mat[i] = preprocess_matrix(matrixd[i].data(), data, *encoder_);
        encoded_mac_mat[i] = preprocess_matrix(mac_matrixd[i].data(), data, *encoder_);
      }
    }
    if (verbose)
      std::cout << "[Server] Preprocess Matrix" << std::endl;


    //Receive ciphertext from client
    vector<Ciphertext> input_ct_0(ct_num);
    recv_encrypted_vector(io, context, input_ct_0);
    // Ciphertext mac_input_ct;
    // recv_ciphertext(io, context, input_ct_0);
    // recv_ciphertext(io, context, mac_input_ct);
    if (verbose)
      cout << "[Server] Receive Ciphertext from client" << endl;

    vector<Ciphertext> input_ct(ct_num);
    vector<Ciphertext> linear(ct_num);
    vector<Ciphertext> linear_mac(ct_num);
    vector<Ciphertext> mac_ver_op(ct_num);
    for(uint64_t i=0; i<ct_num; i++){
      //Add Shares
      evaluator_->add_plain(input_ct_0[i], input_pt[i], input_ct[i]);

      if(i<ct_num-1){
        //Compute FC component
        linear[i] = fc_online(input_ct[i], encoded_mat[i], data_1, *evaluator_, *gal_keys_, *relin_keys_, *zero_);
        // Ciphertext linear_mac = fc_online(mac_input_ct, encoded_mat, data, *evaluator_, *gal_keys_, *relin_keys_, *zero_);
        linear_mac[i] = fc_online(input_ct[i], encoded_mac_mat[i], data_1, *evaluator_, *gal_keys_, *relin_keys_, *zero_);
      }else{
        linear[i] = fc_online(input_ct[i], encoded_mat[i], data, *evaluator_, *gal_keys_, *relin_keys_, *zero_);
        linear_mac[i] = fc_online(input_ct[i], encoded_mac_mat[i], data, *evaluator_, *gal_keys_, *relin_keys_, *zero_);
      }

      evaluator_->multiply_plain(input_ct_0[i], *enc_mac, mac_ver_op[i]);

      //Linear Share
      evaluator_->sub_plain_inplace(linear[i], linear_0[i]);
      //Linear MAC Share
      evaluator_->sub_plain_inplace(linear_mac[i], linear_mac_0[i]);
      //MAC Verification Share
      evaluator->sub_plain_inplace(mac_ver_op[i], mac_ver_shares_0[i]);
    }

    if (verbose)
      cout << "[Server] Generate Client shares" << endl;

    //Send ciphertexts to client
    send_encrypted_vector(io, linear);
    send_encrypted_vector(io, linear_mac);
    send_encrypted_vector(io, mac_ver_op);
    if (verbose)
      cout << "[Server] Send ciphertexts of shares to client" << endl;

    // Verify
    vector<uint64_t> verify_msg(common_dim);
    io->recv_data(verify_msg.data(), sizeof(uint64_t)*common_dim);
    uint64_t ctr = 0;
    for (uint64_t i = 0; i < common_dim; i++){
      uint64_t t1 = i/slot_count;
      uint64_t t2 = i%slot_count;
      if((mod_mult(mac_key, input_vec_local[i], mod) + mac_ver_shares_vec[t1][t2] + (prime_mod - mac_input_vec[t1][t2])) % prime_mod == verify_msg[i])
        ctr++;
      // std::cout << "verify_msg[i]:" << verify_msg[i] << std::endl;
    }
    cout << "Correct input Shares: " << ctr << endl;
    if(ctr < common_dim) cout << "Abort!"  << endl;

    if(verify_output) {
      vector<uint64_t>  input_vec_1(common_dim);
      vector<uint64_t>  mac_input_vec_1(common_dim);
      //receive client input
      io->recv_data(input_vec_1.data(), sizeof(uint64_t)*common_dim);
      io->recv_data(mac_input_vec_1.data(), sizeof(uint64_t)*common_dim);
      for(uint64_t i=0; i<common_dim; i++) {
        input_vec_1[i] = (input_vec_1[i] + input_vec_local[i]) % prime_mod;
        mac_input_vec_1[i] = (mac_input_vec_1[i] + mac_input_vec_local[i]) % prime_mod;
      }

      // server matrix
      vector<uint64_t *> matrixd_local(num_rows);
      vector<uint64_t *> mac_matrixd_local(num_rows);
      for (uint64_t i = 0; i < num_rows; i++) {
        matrixd_local[i] = new uint64_t[common_dim];
        mac_matrixd_local[i] = new uint64_t[common_dim];
        for (uint64_t j = 0; j < common_dim; j++) {
          matrixd_local[i][j] = matrix[i][j];
          mac_matrixd_local[i][j] = mod_mult(mac_key, matrix[i][j], prime_mod);
        }
      }

      //Compute FC
      auto result_actual_input = ideal_functionality(input_vec_1.data(), matrixd_local.data(), num_rows, common_dim, mod);
      auto result_actual_mac = ideal_functionality(mac_input_vec_1.data(), matrixd_local.data(), num_rows, common_dim, mod);

      //receive client_shares
      vector<uint64_t> op_shares_vec_1(num_rows);
      vector<uint64_t> mac_op_shares_vec_1(num_rows);
      io->recv_data(op_shares_vec_1.data(), sizeof(uint64_t)*num_rows);
      io->recv_data(mac_op_shares_vec_1.data(), sizeof(uint64_t)*num_rows);

      // reconstruct output
      vector<uint64_t> sum_op_shares_vec(num_rows, 0);
      vector<uint64_t> sum_mac_op_shares_vec(num_rows, 0);
      for(uint64_t i=0; i<ct_num; i++){
        sum_op_shares_vec[0] = (sum_op_shares_vec[0]+op_shares_vec[i][0])%prime_mod;
        sum_mac_op_shares_vec[0] = (sum_mac_op_shares_vec[0]+mac_op_shares_vec[i][0])%prime_mod;
      }
      for(uint64_t i=0; i<num_rows; i++) {
        op_shares_vec_1[i] = (sum_op_shares_vec[i] + op_shares_vec_1[i])%prime_mod;
        mac_op_shares_vec_1[i] = (sum_mac_op_shares_vec[i] + mac_op_shares_vec_1[i])%prime_mod;
      }

      //check for equality
      uint64_t mctr=0;
      uint64_t mvctr=0;
      for(uint64_t i=0;i<num_rows; i++) {
        if(result_actual_input[i] == op_shares_vec_1[i])
          mctr++;
        if(result_actual_mac[i] == mac_op_shares_vec_1[i])
          mvctr++;
      }
      cout<<"Correct Computation: "<< mctr <<endl;
      cout<<"Correct Mac Shares: "<< mvctr <<endl;
    }
  }
}

void FCField::vector_multiplication_large_no_share_1(uint64_t num_rows, uint64_t common_dim, uint64_t num_cols,
                                    vector<vector<uint64_t>> &matrix,
                                    vector<vector<uint64_t>> &input,
                                    vector<vector<uint64_t>> &mac_M,
                                    vector<vector<uint64_t>> &mac_K,
                                    uint64_t mac_key,
                                    uint64_t *output,
                                    uint64_t *output_mac,
                                    seal::Modulus mod,
                                    bool verify_output, bool verbose, double &wait_time) {
  // mac_M = mac_K + mac_key * input

  assert((num_cols == 1) && (num_rows == 1));
  bool ss_zero = false;
  // 用多个密文块进行加密（密文块数量为ct_num），每个块能够加密的明文的数量为slot_count（取POLY_MOD_DEGREE）
  // 最后一个块可能没有装满，元素个数可能少于POLY_MOD_DEGREE，用data记录最后一个块的情况，data_1记录其余块的情况
  uint64_t ct_num = ceil((double)common_dim / POLY_MOD_DEGREE);
  uint64_t remain = common_dim % POLY_MOD_DEGREE;
  if(remain != 0){
    data.filter_h = num_rows;
    data.filter_w = remain;
    data.image_size = remain;
  }else{
    remain = POLY_MOD_DEGREE;
    data.filter_h = num_rows;
    data.filter_w = POLY_MOD_DEGREE;
    data.image_size = POLY_MOD_DEGREE;
  }
  data_1.filter_h = num_rows;
  data_1.filter_w = POLY_MOD_DEGREE;
  data_1.image_size = POLY_MOD_DEGREE;
  
  this->slot_count = POLY_MOD_DEGREE;
      // min(max(POLY_MOD_DEGREE, 2 * next_pow2(common_dim)), SEAL_POLY_MOD_DEGREE_MAX);
  configure();
  configure_1();

  Encryptor *encryptor_;
  Decryptor *decryptor_;
  Evaluator *evaluator_;
  BatchEncoder *encoder_;
  GaloisKeys *gal_keys_;
  RelinKeys *relin_keys_;
  Ciphertext *zero_;
  if (slot_count > POLY_MOD_DEGREE) {
    generate_new_keys(party, io, slot_count, context, encryptor_, decryptor_,
                      evaluator_, encoder_, gal_keys_, relin_keys_, zero_);
  } else {
    encryptor_ = this->encryptor;
    decryptor_ = this->decryptor;
    evaluator_ = this->evaluator;
    encoder_ = this->encoder;
    gal_keys_ = this->gal_keys;
    relin_keys_ = this->relin_keys;
    zero_ = this->zero;
  }


  if(party == BOB) {
    // 二维数组存储，便于分块加密和计算
    vector<vector<uint64_t>> input_vec(ct_num, vector<uint64_t>(slot_count, 0));
    vector<vector<uint64_t>> mac_M_vec(ct_num, vector<uint64_t>(slot_count, 0));
    for (uint64_t i = 0; i < ct_num; i++) {
      if(i < ct_num-1){
        for(uint64_t j=0; j<slot_count; j++){
          input_vec[i][j] = input[i*slot_count+j][0];
          mac_M_vec[i][j] = mac_M[i*slot_count+j][0];
        }
      }else{
        input_vec[i].resize(remain);
        mac_M_vec[i].resize(remain);
        for(uint64_t j=0; j<remain; j++){
          input_vec[i][j] = input[i*slot_count+j][0];
          mac_M_vec[i][j] = mac_M[i*slot_count+j][0];
        }
      }
    }
    if (verbose)
      std::cout << "[Client] Vector Generated" << std::endl;

    // 加密并发送给server
    vector<Ciphertext> input_ct(ct_num);
    for(uint64_t i=0; i<ct_num; i++){
      if(i<ct_num-1){
        input_ct[i] = preprocess_vec(input_vec[i].data(), data_1, *encryptor_, *encoder_);
      }else{
        input_ct[i] = preprocess_vec(input_vec[i].data(), data, *encryptor_, *encoder_);
      }
    }
    // 多线程并行发送
    // if(num_threads<1){
    //   uint64_t *distribution = new uint64_t[num_threads+1];
    //   uint64_t average = ct_num / num_threads;
    //   uint64_t left = ct_num % num_threads;
    //   distribution[0] = 0;
    //   for(uint64_t i=1; i<=num_threads; i++){
    //     distribution[i] = average + ((left>=i)?1:0);
    //     distribution[i] += distribution[i-1];
    //   }
    //   for(uint64_t i=0; i<num_threads; i++){
    //     if(distribution[i+1]>distribution[i]){
    //       vector<Ciphertext> tmp = CutArrs(input_ct, distribution[i], distribution[i+1]);
    //       send_encrypted_vector(ios[i]->io, tmp);
    //     }
    //   }
    // }else{
    //   send_encrypted_vector(io, input_ct);
    // }
    send_encrypted_vector(io, input_ct);
    // auto mac_input_ct = preprocess_vec(mac_M_vec.data(), data, *encryptor_, *encoder_);
    // send_ciphertext(io, input_ct);
    // send_ciphertext(io, mac_input_ct);

    if (verbose)
      std::cout << "[Client] Vector processed and sent" << std::endl;

    // 接收到server计算的输出并解密
    vector<Ciphertext> linear(ct_num);
    vector<Ciphertext> linear_mac(ct_num);
    vector<Ciphertext> mac_ver_ip(ct_num);
    auto start_time = clock_start();
    recv_encrypted_vector(io, context, linear);
    recv_encrypted_vector(io, context, linear_mac);
    recv_encrypted_vector(io, context, mac_ver_ip);
    wait_time += time_from(start_time);
    if (verbose)
      std::cout << "[Client] Receive ciphertexts of shares" << std::endl;
    // 解密函数返回的是地址
    vector<uint64_t*> linear_1(ct_num);
    vector<uint64_t*> linear_mac_1(ct_num);
    vector<uint64_t*> mac_ver_ip_1_addr(ct_num);
    for(uint64_t i=0; i<ct_num; i++){
      if(i<ct_num-1){
        linear_1[i] = fc_postprocess_1(linear[i], data_1, *encoder_, *decryptor_);
        linear_mac_1[i] = fc_postprocess_1(linear_mac[i], data_1, *encoder_, *decryptor_);
        mac_ver_ip_1_addr[i] = fc_postprocess_mac(mac_ver_ip[i], data_1, *encoder_, *decryptor_);
      }else{
        linear_1[i] = fc_postprocess_1(linear[i], data, *encoder_, *decryptor_);
        linear_mac_1[i] = fc_postprocess_1(linear_mac[i], data, *encoder_, *decryptor_);
        mac_ver_ip_1_addr[i] = fc_postprocess_mac(mac_ver_ip[i], data, *encoder_, *decryptor_);
      }
    }
    if (verbose)
      std::cout << "[Client] Obtain shares" << std::endl;

    // 求和得到向量内积、内积的mac、用于验证的message（注意向量内积、内积的mac都是一个数，而用于验证的message维度与输入相同）
    uint64_t sum_linear_1 = 0;
    uint64_t sum_linear_mac_1 = 0;
    vector<vector<uint64_t>> mac_ver_ip_1(ct_num, vector<uint64_t>(slot_count, 0));
    for(uint64_t i=0; i<ct_num; i++){
      sum_linear_1 = (sum_linear_1+linear_1[i][0])%prime_mod;
      sum_linear_mac_1 = (sum_linear_mac_1+linear_mac_1[i][0])%prime_mod;
      if(i<ct_num-1){
        for(uint64_t j=0; j<slot_count; j++){
          mac_ver_ip_1[i][j] = mac_ver_ip_1_addr[i][j];
        }
      }else{
        mac_ver_ip_1[i].resize(remain);
        for(uint64_t j=0; j<remain; j++){
          mac_ver_ip_1[i][j] = mac_ver_ip_1_addr[i][j];
        }
      }
    }
    *output = sum_linear_1;
    *output_mac = sum_linear_mac_1;

    // Verify：输入没有被篡改
    vector<uint64_t> verify_msg(common_dim);
    for(uint64_t i=0; i<ct_num; i++){
      if(i<ct_num-1){
        for(uint64_t j=0; j<slot_count; j++){
          verify_msg[i*slot_count+j] = (mac_M_vec[i][j] + (prime_mod - mac_ver_ip_1[i][j])) % prime_mod;
        }
      }else{
        for(uint64_t j=0; j<remain; j++){
          verify_msg[i*slot_count+j] = (mac_M_vec[i][j] + (prime_mod - mac_ver_ip_1[i][j])) % prime_mod;
        }
      }
    }
    if (verbose)
      std::cout << "[Client] Verify message Generated" << std::endl;
    io->send_data(verify_msg.data(), sizeof(uint64_t) * common_dim);
    
    if(verify_output) { // 结果正确性校验
      // 一维数组存储在本地，便于遍历
      vector<uint64_t> input_vec_local(common_dim);
      vector<uint64_t> mac_M_local(common_dim);
      for (uint64_t i = 0; i < common_dim; i++) {
        input_vec_local[i] = input[i][0];
        mac_M_local[i] = mac_M[i][0];
      }

      io->send_data(input_vec_local.data(), sizeof(uint64_t) * common_dim);
      io->send_data(mac_M_local.data(), sizeof(uint64_t) * common_dim);
      io->send_data(&sum_linear_1, sizeof(uint64_t) * num_rows);
      io->send_data(&sum_linear_mac_1, sizeof(uint64_t) * num_rows);
    }
  } else {
    // compute plaintext of delta
    vector<uint64_t> mac_vec(encoder_->slot_count(), mac_key);
    Plaintext* enc_mac = new Plaintext();
    encoder_->encode(mac_vec, *enc_mac);

    //Generate random_shares
    PRG prg;
    vector<vector<uint64_t>> op_shares_vec(ct_num, vector<uint64_t>(slot_count, 0));
    vector<vector<uint64_t>> mac_op_shares_vec(ct_num, vector<uint64_t>(slot_count, 0));
    vector<vector<uint64_t>> mac_ver_shares_vec(ct_num, vector<uint64_t>(slot_count, 0));
    vector<Plaintext> linear_0(ct_num);
    vector<Plaintext> linear_mac_0(ct_num);
    vector<Plaintext> mac_ver_shares_0(ct_num);
    for(uint64_t i=0; i<ct_num; i++){
      if(i<ct_num-1){
        if(!ss_zero) {
          random_mod_p(prg, op_shares_vec[i].data(), slot_count, prime_mod);
          random_mod_p(prg, mac_op_shares_vec[i].data(), slot_count, prime_mod);
          random_mod_p(prg, mac_ver_shares_vec[i].data(), slot_count, prime_mod);
        }
        linear_0[i] = preprocess_vec_plain(op_shares_vec[i].data(), data_1, *encoder_);
        linear_mac_0[i] = preprocess_vec_plain(mac_op_shares_vec[i].data(), data_1, *encoder_);
        mac_ver_shares_0[i] = preprocess_vec_plain(mac_ver_shares_vec[i].data(), data_1, *encoder_);
      }else{
        mac_ver_shares_vec[i].resize(remain);
        if(!ss_zero) {
          random_mod_p(prg, op_shares_vec[i].data(), remain, prime_mod);
          random_mod_p(prg, mac_op_shares_vec[i].data(), remain, prime_mod);
          random_mod_p(prg, mac_ver_shares_vec[i].data(), remain, prime_mod);
        }
        linear_0[i] = preprocess_vec_plain(op_shares_vec[i].data(), data, *encoder_);
        linear_mac_0[i] = preprocess_vec_plain(mac_op_shares_vec[i].data(), data, *encoder_);
        mac_ver_shares_0[i] = preprocess_vec_plain(mac_ver_shares_vec[i].data(), data, *encoder_);
      }
    }
    
    if (verbose)
      cout << "[Server] Generate shares" << endl;

    //Preprocess Matrix
    vector<vector<uint64_t *>> matrixd(ct_num, vector<uint64_t *>(num_rows));
    vector<vector<uint64_t *>> mac_matrixd(ct_num, vector<uint64_t *>(num_rows));
    vector<vector<Plaintext>> encoded_mat(ct_num);
    vector<vector<Plaintext>> encoded_mac_mat(ct_num);
    for(uint64_t i=0; i<ct_num; i++){
      if(i<ct_num-1){
        for (uint64_t j = 0; j < num_rows; j++) {
          matrixd[i][j] = new uint64_t[slot_count];
          mac_matrixd[i][j] = new uint64_t[slot_count];
          for (uint64_t k = 0; k < slot_count; k++) {
            matrixd[i][j][k] = matrix[j][i*slot_count+k];
            mac_matrixd[i][j][k] = mod_mult(mac_key, matrix[j][i*slot_count+k], prime_mod);
          }
        }
        encoded_mat[i] = preprocess_matrix(matrixd[i].data(), data_1, *encoder_);
        encoded_mac_mat[i] = preprocess_matrix(mac_matrixd[i].data(), data_1, *encoder_);
      }else{
        for (uint64_t j = 0; j < num_rows; j++) {
          matrixd[i][j] = new uint64_t[remain];
          mac_matrixd[i][j] = new uint64_t[remain];
          for (uint64_t k = 0; k < remain; k++) {
            matrixd[i][j][k] = matrix[j][i*slot_count+k];
            mac_matrixd[i][j][k] = mod_mult(mac_key, matrix[j][i*slot_count+k], prime_mod);
          }
        }
        encoded_mat[i] = preprocess_matrix(matrixd[i].data(), data, *encoder_);
        encoded_mac_mat[i] = preprocess_matrix(mac_matrixd[i].data(), data, *encoder_);
      }
    }
    if (verbose)
      std::cout << "[Server] Preprocess Matrix" << std::endl;


    //Receive ciphertext from client
    vector<Ciphertext> input_ct(ct_num);
    // if(num_threads<1){
    //   uint64_t *distribution = new uint64_t[num_threads];
    //   uint64_t average = ct_num / num_threads;
    //   uint64_t left = ct_num % num_threads;
    //   for(uint64_t i=1; i<=num_threads; i++){
    //     distribution[i] = average + ((left>=i)?1:0);
    //   }
    //   vector<vector<Ciphertext>> tmp(num_threads);
    //   for(uint64_t i=0; i<num_threads; i++){
    //     if(distribution[i]>0){
    //       tmp[i].resize(distribution[i]);
    //       recv_encrypted_vector(ios[i]->io, context, tmp[i]);
    //     }
    //   }
    //   for(uint64_t i=0; i<num_threads; i++){
    //     if(distribution[i]>0) input_ct.insert(input_ct.end(), tmp[i].begin(), tmp[i].end());
    //   }
    // }else{
    //   recv_encrypted_vector(io, context, input_ct);
    // }
    auto start_time = clock_start();
    recv_encrypted_vector(io, context, input_ct);
    wait_time += time_from(start_time);
    if (verbose)
      cout << "[Server] Receive Ciphertext from client" << endl;

    // compute c1 c2 c3
    vector<Ciphertext> linear(ct_num);
    vector<Ciphertext> linear_mac(ct_num);
    vector<Ciphertext> mac_ver_op(ct_num);
    for(uint64_t i=0; i<ct_num; i++){
      if(i<ct_num-1){
        //Compute FC component
        linear[i] = fc_online_1(input_ct[i], encoded_mat[i], data_1, *evaluator_, *gal_keys_, *relin_keys_, *zero_);
        // Ciphertext linear_mac = fc_online(mac_input_ct, encoded_mat, data, *evaluator_, *gal_keys_, *relin_keys_, *zero_);
        linear_mac[i] = fc_online_1(input_ct[i], encoded_mac_mat[i], data_1, *evaluator_, *gal_keys_, *relin_keys_, *zero_);
      }else{
        linear[i] = fc_online_1(input_ct[i], encoded_mat[i], data, *evaluator_, *gal_keys_, *relin_keys_, *zero_);
        linear_mac[i] = fc_online_1(input_ct[i], encoded_mac_mat[i], data, *evaluator_, *gal_keys_, *relin_keys_, *zero_);
      }

      evaluator_->multiply_plain(input_ct[i], *enc_mac, mac_ver_op[i]);

      //Linear Share
      evaluator_->sub_plain_inplace(linear[i], linear_0[i]);
      //Linear MAC Share
      evaluator_->sub_plain_inplace(linear_mac[i], linear_mac_0[i]);
      //MAC Verification Share
      evaluator->sub_plain_inplace(mac_ver_op[i], mac_ver_shares_0[i]);
    }

    if (verbose)
      cout << "[Server] Generate Client shares" << endl;

    //Send ciphertexts to client
    send_encrypted_vector(io, linear);
    send_encrypted_vector(io, linear_mac);
    send_encrypted_vector(io, mac_ver_op);
    if (verbose)
      cout << "[Server] Send ciphertexts of shares to client" << endl;

    vector<uint64_t> sum_op_shares_vec(num_rows, 0);
    vector<uint64_t> sum_mac_op_shares_vec(num_rows, 0);
    for(uint64_t i=0; i<common_dim; i++){
      sum_op_shares_vec[0] = (sum_op_shares_vec[0]+op_shares_vec[i/slot_count][i%slot_count])%prime_mod;
      sum_mac_op_shares_vec[0] = (sum_mac_op_shares_vec[0]+mac_op_shares_vec[i/slot_count][i%slot_count])%prime_mod;
    }
    *output = sum_op_shares_vec[0];
    *output_mac = sum_mac_op_shares_vec[0];

    // Verify
    vector<uint64_t> mac_K_local(common_dim);
    for (uint64_t i = 0; i < common_dim; i++) {
      mac_K_local[i] = mac_K[i][0];
    }
    vector<uint64_t> verify_msg(common_dim);
    start_time = clock_start();
    io->recv_data(verify_msg.data(), sizeof(uint64_t)*common_dim);
    wait_time += time_from(start_time);
    uint64_t ctr = 0;
    for (uint64_t i = 0; i < common_dim; i++){
      uint64_t t1 = i/slot_count;
      uint64_t t2 = i%slot_count;
      if((mac_K_local[i] + mac_ver_shares_vec[t1][t2]) % prime_mod == verify_msg[i])
        ctr++;
      // std::cout << "verify_msg[i]:" << verify_msg[i] << std::endl;
    }
    if (verbose) cout << "Correct input Shares: " << ctr << endl;
    if(ctr < common_dim) cout << "Abort!"  << endl;

    if(verify_output) {
      vector<uint64_t> input_vec(common_dim);
      vector<uint64_t> mac_M_vec(common_dim);
      //receive client input
      io->recv_data(input_vec.data(), sizeof(uint64_t)*common_dim);
      io->recv_data(mac_M_vec.data(), sizeof(uint64_t)*common_dim);

      // server matrix
      vector<uint64_t *> matrixd_local(num_rows);
      vector<uint64_t *> mac_matrixd_local(num_rows);
      for (uint64_t i = 0; i < num_rows; i++) {
        matrixd_local[i] = new uint64_t[common_dim];
        mac_matrixd_local[i] = new uint64_t[common_dim];
        for (uint64_t j = 0; j < common_dim; j++) {
          matrixd_local[i][j] = matrix[i][j];
          mac_matrixd_local[i][j] = mod_mult(mac_key, matrix[i][j], prime_mod);
        }
      }

      //Compute FC
      auto result_actual_output = ideal_functionality(input_vec.data(), matrixd_local.data(), num_rows, common_dim, mod);
      auto result_actual_mac = ideal_functionality(input_vec.data(), mac_matrixd_local.data(), num_rows, common_dim, mod);

      //receive client_shares
      vector<uint64_t> op_shares_vec_1(num_rows);
      vector<uint64_t> mac_op_shares_vec_1(num_rows);
      io->recv_data(op_shares_vec_1.data(), sizeof(uint64_t)*num_rows);
      io->recv_data(mac_op_shares_vec_1.data(), sizeof(uint64_t)*num_rows);

      // reconstruct output
      for(uint64_t i=0; i<num_rows; i++) {
        op_shares_vec_1[i] = (sum_op_shares_vec[i] + op_shares_vec_1[i])%prime_mod;
        mac_op_shares_vec_1[i] = (sum_mac_op_shares_vec[i] + mac_op_shares_vec_1[i])%prime_mod;
      }

      //check for equality
      uint64_t mctr=0;
      uint64_t mvctr=0;
      for(uint64_t i=0;i<num_rows; i++) {
        if(result_actual_output[i] == op_shares_vec_1[i])
          mctr++;
        if(result_actual_mac[i] == mac_op_shares_vec_1[i])
          mvctr++;
      }
      if (verbose) cout<<"Correct Computation: "<< mctr <<endl;
      if (verbose) cout<<"Correct Mac Shares: "<< mvctr <<endl;
      if(mctr < num_rows) cout << "Computation error! Abort!"  << endl;
      if(mvctr < num_rows) cout << "Mac error! Abort!"  << endl;
    }
  }
}

void FCField::vector_multiplication_large_no_share(uint64_t num_rows, uint64_t common_dim, uint64_t num_cols,
                                    vector<vector<uint64_t>> &matrix,
                                    vector<vector<uint64_t>> &input,
                                    vector<vector<uint64_t>> &mac_M,
                                    vector<vector<uint64_t>> &mac_K,
                                    uint64_t mac_key,
                                    uint64_t *output,
                                    uint64_t *output_mac,
                                    seal::Modulus mod,
                                    bool verify_output, bool verbose) {
  // mac_M = mac_K + mac_key * input

  assert((num_cols == 1) && (num_rows == 1));
  bool ss_zero = false;
  // 用多个密文块进行加密（密文块数量为ct_num），每个块能够加密的明文的数量为slot_count（取POLY_MOD_DEGREE）
  // 最后一个块可能没有装满，元素个数可能少于POLY_MOD_DEGREE，用data记录最后一个块的情况，data_1记录其余块的情况
  uint64_t ct_num = ceil((double)common_dim / POLY_MOD_DEGREE);
  uint64_t remain = common_dim % POLY_MOD_DEGREE;
  if(remain != 0){
    data.filter_h = num_rows;
    data.filter_w = remain;
    data.image_size = remain;
  }else{
    remain = POLY_MOD_DEGREE;
    data.filter_h = num_rows;
    data.filter_w = POLY_MOD_DEGREE;
    data.image_size = POLY_MOD_DEGREE;
  }
  data_1.filter_h = num_rows;
  data_1.filter_w = POLY_MOD_DEGREE;
  data_1.image_size = POLY_MOD_DEGREE;
  
  this->slot_count = POLY_MOD_DEGREE;
      // min(max(POLY_MOD_DEGREE, 2 * next_pow2(common_dim)), SEAL_POLY_MOD_DEGREE_MAX);
  configure();
  configure_1();

  Encryptor *encryptor_;
  Decryptor *decryptor_;
  Evaluator *evaluator_;
  BatchEncoder *encoder_;
  GaloisKeys *gal_keys_;
  RelinKeys *relin_keys_;
  Ciphertext *zero_;
  if (slot_count > POLY_MOD_DEGREE) {
    generate_new_keys(party, io, slot_count, context, encryptor_, decryptor_,
                      evaluator_, encoder_, gal_keys_, relin_keys_, zero_);
  } else {
    encryptor_ = this->encryptor;
    decryptor_ = this->decryptor;
    evaluator_ = this->evaluator;
    encoder_ = this->encoder;
    gal_keys_ = this->gal_keys;
    relin_keys_ = this->relin_keys;
    zero_ = this->zero;
  }


  if(party == BOB) {
    // 二维数组存储，便于分块加密和计算
    vector<vector<uint64_t>> input_vec(ct_num, vector<uint64_t>(slot_count, 0));
    vector<vector<uint64_t>> mac_M_vec(ct_num, vector<uint64_t>(slot_count, 0));
    for (uint64_t i = 0; i < ct_num; i++) {
      if(i < ct_num-1){
        for(uint64_t j=0; j<slot_count; j++){
          input_vec[i][j] = input[i*slot_count+j][0];
          mac_M_vec[i][j] = mac_M[i*slot_count+j][0];
        }
      }else{
        input_vec[i].resize(remain);
        mac_M_vec[i].resize(remain);
        for(uint64_t j=0; j<remain; j++){
          input_vec[i][j] = input[i*slot_count+j][0];
          mac_M_vec[i][j] = mac_M[i*slot_count+j][0];
        }
      }
    }
    if (verbose)
      std::cout << "[Client] Vector Generated" << std::endl;

    // 加密并发送给server
    vector<Ciphertext> input_ct(ct_num);
    for(uint64_t i=0; i<ct_num; i++){
      if(i<ct_num-1){
        input_ct[i] = preprocess_vec(input_vec[i].data(), data_1, *encryptor_, *encoder_);
      }else{
        input_ct[i] = preprocess_vec(input_vec[i].data(), data, *encryptor_, *encoder_);
      }
    }
    // 多线程并行发送
    // if(num_threads<1){
    //   uint64_t *distribution = new uint64_t[num_threads+1];
    //   uint64_t average = ct_num / num_threads;
    //   uint64_t left = ct_num % num_threads;
    //   distribution[0] = 0;
    //   for(uint64_t i=1; i<=num_threads; i++){
    //     distribution[i] = average + ((left>=i)?1:0);
    //     distribution[i] += distribution[i-1];
    //   }
    //   for(uint64_t i=0; i<num_threads; i++){
    //     if(distribution[i+1]>distribution[i]){
    //       vector<Ciphertext> tmp = CutArrs(input_ct, distribution[i], distribution[i+1]);
    //       send_encrypted_vector(ios[i]->io, tmp);
    //     }
    //   }
    // }else{
    //   send_encrypted_vector(io, input_ct);
    // }
    send_encrypted_vector(io, input_ct);
    // auto mac_input_ct = preprocess_vec(mac_M_vec.data(), data, *encryptor_, *encoder_);
    // send_ciphertext(io, input_ct);
    // send_ciphertext(io, mac_input_ct);

    if (verbose)
      std::cout << "[Client] Vector processed and sent" << std::endl;

    // 接收到server计算的输出并解密
    vector<Ciphertext> linear(ct_num);
    vector<Ciphertext> linear_mac(ct_num);
    vector<Ciphertext> mac_ver_ip(ct_num);
    recv_encrypted_vector(io, context, linear);
    recv_encrypted_vector(io, context, linear_mac);
    recv_encrypted_vector(io, context, mac_ver_ip);
    if (verbose)
      std::cout << "[Client] Receive ciphertexts of shares" << std::endl;
    // 解密函数返回的是地址
    vector<uint64_t*> linear_1(ct_num);
    vector<uint64_t*> linear_mac_1(ct_num);
    vector<uint64_t*> mac_ver_ip_1_addr(ct_num);
    for(uint64_t i=0; i<ct_num; i++){
      if(i<ct_num-1){
        linear_1[i] = fc_postprocess(linear[i], data_1, *encoder_, *decryptor_);
        linear_mac_1[i] = fc_postprocess(linear_mac[i], data_1, *encoder_, *decryptor_);
        mac_ver_ip_1_addr[i] = fc_postprocess_mac(mac_ver_ip[i], data_1, *encoder_, *decryptor_);
      }else{
        linear_1[i] = fc_postprocess(linear[i], data, *encoder_, *decryptor_);
        linear_mac_1[i] = fc_postprocess(linear_mac[i], data, *encoder_, *decryptor_);
        mac_ver_ip_1_addr[i] = fc_postprocess_mac(mac_ver_ip[i], data, *encoder_, *decryptor_);
      }
    }
    if (verbose)
      std::cout << "[Client] Obtain shares" << std::endl;

    // 求和得到向量内积、内积的mac、用于验证的message（注意向量内积、内积的mac都是一个数，而用于验证的message维度与输入相同）
    uint64_t sum_linear_1 = 0;
    uint64_t sum_linear_mac_1 = 0;
    vector<vector<uint64_t>> mac_ver_ip_1(ct_num, vector<uint64_t>(slot_count, 0));
    for(uint64_t i=0; i<ct_num; i++){
      sum_linear_1 = (sum_linear_1+linear_1[i][0])%prime_mod;
      sum_linear_mac_1 = (sum_linear_mac_1+linear_mac_1[i][0])%prime_mod;
      if(i<ct_num-1){
        for(uint64_t j=0; j<slot_count; j++){
          mac_ver_ip_1[i][j] = mac_ver_ip_1_addr[i][j];
        }
      }else{
        mac_ver_ip_1[i].resize(remain);
        for(uint64_t j=0; j<remain; j++){
          mac_ver_ip_1[i][j] = mac_ver_ip_1_addr[i][j];
        }
      }
    }
    *output = sum_linear_1;
    *output_mac = sum_linear_mac_1;

    // Verify：输入没有被篡改
    vector<uint64_t> verify_msg(common_dim);
    for(uint64_t i=0; i<ct_num; i++){
      if(i<ct_num-1){
        for(uint64_t j=0; j<slot_count; j++){
          verify_msg[i*slot_count+j] = (mac_M_vec[i][j] + (prime_mod - mac_ver_ip_1[i][j])) % prime_mod;
        }
      }else{
        for(uint64_t j=0; j<remain; j++){
          verify_msg[i*slot_count+j] = (mac_M_vec[i][j] + (prime_mod - mac_ver_ip_1[i][j])) % prime_mod;
        }
      }
    }
    if (verbose)
      std::cout << "[Client] Verify message Generated" << std::endl;
    io->send_data(verify_msg.data(), sizeof(uint64_t) * common_dim);
    
    if(verify_output) { // 结果正确性校验
      // 一维数组存储在本地，便于遍历
      vector<uint64_t> input_vec_local(common_dim);
      vector<uint64_t> mac_M_local(common_dim);
      for (uint64_t i = 0; i < common_dim; i++) {
        input_vec_local[i] = input[i][0];
        mac_M_local[i] = mac_M[i][0];
      }

      io->send_data(input_vec_local.data(), sizeof(uint64_t) * common_dim);
      io->send_data(mac_M_local.data(), sizeof(uint64_t) * common_dim);
      io->send_data(&sum_linear_1, sizeof(uint64_t) * num_rows);
      io->send_data(&sum_linear_mac_1, sizeof(uint64_t) * num_rows);
    }
  } else {
    // compute plaintext of delta
    vector<uint64_t> mac_vec(encoder_->slot_count(), mac_key);
    Plaintext* enc_mac = new Plaintext();
    encoder_->encode(mac_vec, *enc_mac);

    //Generate random_shares
    PRG prg;
    vector<vector<uint64_t>> op_shares_vec(ct_num, vector<uint64_t>(num_rows, 0));
    vector<vector<uint64_t>> mac_op_shares_vec(ct_num, vector<uint64_t>(num_rows, 0));
    vector<vector<uint64_t>> mac_ver_shares_vec(ct_num, vector<uint64_t>(slot_count, 0));
    vector<Plaintext> linear_0(ct_num);
    vector<Plaintext> linear_mac_0(ct_num);
    vector<Plaintext> mac_ver_shares_0(ct_num);
    for(uint64_t i=0; i<ct_num; i++){
      if(!ss_zero){
        random_mod_p(prg, op_shares_vec[i].data(), num_rows, prime_mod);
        random_mod_p(prg, mac_op_shares_vec[i].data(), num_rows, prime_mod);
      }

      if(i<ct_num-1){
        if(!ss_zero) random_mod_p(prg, mac_ver_shares_vec[i].data(), slot_count, prime_mod);
        linear_0[i] = fc_preprocess_noise(op_shares_vec[i].data(), data_1, *encoder_);
        linear_mac_0[i] = fc_preprocess_noise(mac_op_shares_vec[i].data(), data_1, *encoder_);
        mac_ver_shares_0[i] = preprocess_vec_plain(mac_ver_shares_vec[i].data(), data_1, *encoder_);
      }else{
        mac_ver_shares_vec[i].resize(remain);
        if(!ss_zero) random_mod_p(prg, mac_ver_shares_vec[i].data(), remain, prime_mod);
        linear_0[i] = fc_preprocess_noise(op_shares_vec[i].data(), data, *encoder_);
        linear_mac_0[i] = fc_preprocess_noise(mac_op_shares_vec[i].data(), data, *encoder_);
        mac_ver_shares_0[i] = preprocess_vec_plain(mac_ver_shares_vec[i].data(), data, *encoder_);
      }
    }
    
    if (verbose)
      cout << "[Server] Generate shares" << endl;

    //Preprocess Matrix
    vector<vector<uint64_t *>> matrixd(ct_num, vector<uint64_t *>(num_rows));
    vector<vector<uint64_t *>> mac_matrixd(ct_num, vector<uint64_t *>(num_rows));
    vector<vector<Plaintext>> encoded_mat(ct_num);
    vector<vector<Plaintext>> encoded_mac_mat(ct_num);
    for(uint64_t i=0; i<ct_num; i++){
      if(i<ct_num-1){
        for (uint64_t j = 0; j < num_rows; j++) {
          matrixd[i][j] = new uint64_t[slot_count];
          mac_matrixd[i][j] = new uint64_t[slot_count];
          for (uint64_t k = 0; k < slot_count; k++) {
            matrixd[i][j][k] = matrix[j][i*slot_count+k];
            mac_matrixd[i][j][k] = mod_mult(mac_key, matrix[j][i*slot_count+k], prime_mod);
          }
        }
        encoded_mat[i] = preprocess_matrix(matrixd[i].data(), data_1, *encoder_);
        encoded_mac_mat[i] = preprocess_matrix(mac_matrixd[i].data(), data_1, *encoder_);
      }else{
        for (uint64_t j = 0; j < num_rows; j++) {
          matrixd[i][j] = new uint64_t[remain];
          mac_matrixd[i][j] = new uint64_t[remain];
          for (uint64_t k = 0; k < remain; k++) {
            matrixd[i][j][k] = matrix[j][i*slot_count+k];
            mac_matrixd[i][j][k] = mod_mult(mac_key, matrix[j][i*slot_count+k], prime_mod);
          }
        }
        encoded_mat[i] = preprocess_matrix(matrixd[i].data(), data, *encoder_);
        encoded_mac_mat[i] = preprocess_matrix(mac_matrixd[i].data(), data, *encoder_);
      }
    }
    if (verbose)
      std::cout << "[Server] Preprocess Matrix" << std::endl;


    //Receive ciphertext from client
    vector<Ciphertext> input_ct(ct_num);
    // if(num_threads<1){
    //   uint64_t *distribution = new uint64_t[num_threads];
    //   uint64_t average = ct_num / num_threads;
    //   uint64_t left = ct_num % num_threads;
    //   for(uint64_t i=1; i<=num_threads; i++){
    //     distribution[i] = average + ((left>=i)?1:0);
    //   }
    //   vector<vector<Ciphertext>> tmp(num_threads);
    //   for(uint64_t i=0; i<num_threads; i++){
    //     if(distribution[i]>0){
    //       tmp[i].resize(distribution[i]);
    //       recv_encrypted_vector(ios[i]->io, context, tmp[i]);
    //     }
    //   }
    //   for(uint64_t i=0; i<num_threads; i++){
    //     if(distribution[i]>0) input_ct.insert(input_ct.end(), tmp[i].begin(), tmp[i].end());
    //   }
    // }else{
    //   recv_encrypted_vector(io, context, input_ct);
    // }
    recv_encrypted_vector(io, context, input_ct);
    if (verbose)
      cout << "[Server] Receive Ciphertext from client" << endl;

    // compute c1 c2 c3
    vector<Ciphertext> linear(ct_num);
    vector<Ciphertext> linear_mac(ct_num);
    vector<Ciphertext> mac_ver_op(ct_num);
    for(uint64_t i=0; i<ct_num; i++){
      if(i<ct_num-1){
        //Compute FC component
        linear[i] = fc_online(input_ct[i], encoded_mat[i], data_1, *evaluator_, *gal_keys_, *relin_keys_, *zero_);
        // Ciphertext linear_mac = fc_online(mac_input_ct, encoded_mat, data, *evaluator_, *gal_keys_, *relin_keys_, *zero_);
        linear_mac[i] = fc_online(input_ct[i], encoded_mac_mat[i], data_1, *evaluator_, *gal_keys_, *relin_keys_, *zero_);
      }else{
        linear[i] = fc_online(input_ct[i], encoded_mat[i], data, *evaluator_, *gal_keys_, *relin_keys_, *zero_);
        linear_mac[i] = fc_online(input_ct[i], encoded_mac_mat[i], data, *evaluator_, *gal_keys_, *relin_keys_, *zero_);
      }

      evaluator_->multiply_plain(input_ct[i], *enc_mac, mac_ver_op[i]);

      //Linear Share
      evaluator_->sub_plain_inplace(linear[i], linear_0[i]);
      //Linear MAC Share
      evaluator_->sub_plain_inplace(linear_mac[i], linear_mac_0[i]);
      //MAC Verification Share
      evaluator->sub_plain_inplace(mac_ver_op[i], mac_ver_shares_0[i]);
    }

    if (verbose)
      cout << "[Server] Generate Client shares" << endl;

    //Send ciphertexts to client
    send_encrypted_vector(io, linear);
    send_encrypted_vector(io, linear_mac);
    send_encrypted_vector(io, mac_ver_op);
    if (verbose)
      cout << "[Server] Send ciphertexts of shares to client" << endl;

    vector<uint64_t> sum_op_shares_vec(num_rows, 0);
    vector<uint64_t> sum_mac_op_shares_vec(num_rows, 0);
    for(uint64_t i=0; i<ct_num; i++){
      sum_op_shares_vec[0] = (sum_op_shares_vec[0]+op_shares_vec[i][0])%prime_mod;
      sum_mac_op_shares_vec[0] = (sum_mac_op_shares_vec[0]+mac_op_shares_vec[i][0])%prime_mod;
    }
    *output = sum_op_shares_vec[0];
    *output_mac = sum_mac_op_shares_vec[0];

    // Verify
    vector<uint64_t> mac_K_local(common_dim);
    for (uint64_t i = 0; i < common_dim; i++) {
      mac_K_local[i] = mac_K[i][0];
    }
    vector<uint64_t> verify_msg(common_dim);
    io->recv_data(verify_msg.data(), sizeof(uint64_t)*common_dim);
    uint64_t ctr = 0;
    for (uint64_t i = 0; i < common_dim; i++){
      uint64_t t1 = i/slot_count;
      uint64_t t2 = i%slot_count;
      if((mac_K_local[i] + mac_ver_shares_vec[t1][t2]) % prime_mod == verify_msg[i])
        ctr++;
      // std::cout << "verify_msg[i]:" << verify_msg[i] << std::endl;
    }
    if (verbose) cout << "Correct input Shares: " << ctr << endl;
    if(ctr < common_dim) cout << "Abort!"  << endl;

    if(verify_output) {
      vector<uint64_t> input_vec(common_dim);
      vector<uint64_t> mac_M_vec(common_dim);
      //receive client input
      io->recv_data(input_vec.data(), sizeof(uint64_t)*common_dim);
      io->recv_data(mac_M_vec.data(), sizeof(uint64_t)*common_dim);

      // server matrix
      vector<uint64_t *> matrixd_local(num_rows);
      vector<uint64_t *> mac_matrixd_local(num_rows);
      for (uint64_t i = 0; i < num_rows; i++) {
        matrixd_local[i] = new uint64_t[common_dim];
        mac_matrixd_local[i] = new uint64_t[common_dim];
        for (uint64_t j = 0; j < common_dim; j++) {
          matrixd_local[i][j] = matrix[i][j];
          mac_matrixd_local[i][j] = mod_mult(mac_key, matrix[i][j], prime_mod);
        }
      }

      //Compute FC
      auto result_actual_output = ideal_functionality(input_vec.data(), matrixd_local.data(), num_rows, common_dim, mod);
      auto result_actual_mac = ideal_functionality(input_vec.data(), mac_matrixd_local.data(), num_rows, common_dim, mod);

      //receive client_shares
      vector<uint64_t> op_shares_vec_1(num_rows);
      vector<uint64_t> mac_op_shares_vec_1(num_rows);
      io->recv_data(op_shares_vec_1.data(), sizeof(uint64_t)*num_rows);
      io->recv_data(mac_op_shares_vec_1.data(), sizeof(uint64_t)*num_rows);

      // reconstruct output
      for(uint64_t i=0; i<num_rows; i++) {
        op_shares_vec_1[i] = (sum_op_shares_vec[i] + op_shares_vec_1[i])%prime_mod;
        mac_op_shares_vec_1[i] = (sum_mac_op_shares_vec[i] + mac_op_shares_vec_1[i])%prime_mod;
      }

      //check for equality
      uint64_t mctr=0;
      uint64_t mvctr=0;
      for(uint64_t i=0;i<num_rows; i++) {
        if(result_actual_output[i] == op_shares_vec_1[i])
          mctr++;
        if(result_actual_mac[i] == mac_op_shares_vec_1[i])
          mvctr++;
      }
      if (verbose) cout<<"Correct Computation: "<< mctr <<endl;
      if (verbose) cout<<"Correct Mac Shares: "<< mvctr <<endl;
      if(mctr < num_rows) cout << "Computation error! Abort!"  << endl;
      if(mvctr < num_rows) cout << "Mac error! Abort!"  << endl;
    }
  }
}

void FCField::vector_bool_multiplication_large_no_share(uint64_t num_rows, uint64_t common_dim, uint64_t num_cols,
                                    uint64_t mux,
                                    vector<vector<uint64_t>> &input,
                                    vector<vector<uint64_t>> &mac_M,
                                    vector<vector<uint64_t>> &mac_K,
                                    uint64_t mac_key,
                                    uint64_t *output,
                                    uint64_t *output_mac,
                                    seal::Modulus mod,
                                    bool verify_output, bool verbose, double &wait_time) {

  assert((num_cols == 1) && (num_rows == 1));
  bool ss_zero = false;
  // 用多个密文块进行加密（密文块数量为ct_num），每个块能够加密的明文的数量为slot_count（取POLY_MOD_DEGREE）
  // 最后一个块可能没有装满，元素个数可能少于POLY_MOD_DEGREE，用data记录最后一个块的情况，data_1记录其余块的情况
  uint64_t ct_num = ceil((double)common_dim / POLY_MOD_DEGREE);
  uint64_t remain = common_dim % POLY_MOD_DEGREE;
  if(remain != 0){
    data.filter_h = num_rows;
    data.filter_w = remain;
    data.image_size = remain;
  }else{
    remain = POLY_MOD_DEGREE;
    data.filter_h = num_rows;
    data.filter_w = POLY_MOD_DEGREE;
    data.image_size = POLY_MOD_DEGREE;
  }
  data_1.filter_h = num_rows;
  data_1.filter_w = POLY_MOD_DEGREE;
  data_1.image_size = POLY_MOD_DEGREE;
  
  this->slot_count = POLY_MOD_DEGREE;
      // min(max(POLY_MOD_DEGREE, 2 * next_pow2(common_dim)), SEAL_POLY_MOD_DEGREE_MAX);
  configure();
  configure_1();

  Encryptor *encryptor_;
  Decryptor *decryptor_;
  Evaluator *evaluator_;
  BatchEncoder *encoder_;
  GaloisKeys *gal_keys_;
  RelinKeys *relin_keys_;
  Ciphertext *zero_;
  if (slot_count > POLY_MOD_DEGREE) {
    generate_new_keys(party, io, slot_count, context, encryptor_, decryptor_,
                      evaluator_, encoder_, gal_keys_, relin_keys_, zero_);
  } else {
    encryptor_ = this->encryptor;
    decryptor_ = this->decryptor;
    evaluator_ = this->evaluator;
    encoder_ = this->encoder;
    gal_keys_ = this->gal_keys;
    relin_keys_ = this->relin_keys;
    zero_ = this->zero;
  }


  if(party == BOB) {
    // 二维数组存储，便于分块加密和计算
    vector<vector<uint64_t>> input_vec(ct_num, vector<uint64_t>(slot_count, 0));
    vector<vector<uint64_t>> mac_M_vec(ct_num, vector<uint64_t>(slot_count, 0));
    for (uint64_t i = 0; i < ct_num; i++) {
      if(i < ct_num-1){
        for(uint64_t j=0; j<slot_count; j++){
          input_vec[i][j] = input[i*slot_count+j][0];
          mac_M_vec[i][j] = mac_M[i*slot_count+j][0];
        }
      }else{
        input_vec[i].resize(remain);
        mac_M_vec[i].resize(remain);
        for(uint64_t j=0; j<remain; j++){
          input_vec[i][j] = input[i*slot_count+j][0];
          mac_M_vec[i][j] = mac_M[i*slot_count+j][0];
        }
      }
    }
    if (verbose)
      std::cout << "[Client] Vector Generated" << std::endl;

    // 加密并发送给server
    vector<Ciphertext> input_ct(ct_num);
    for(uint64_t i=0; i<ct_num; i++){
      if(i<ct_num-1){
        input_ct[i] = preprocess_vec(input_vec[i].data(), data_1, *encryptor_, *encoder_);
      }else{
        input_ct[i] = preprocess_vec(input_vec[i].data(), data, *encryptor_, *encoder_);
      }
    }
    send_encrypted_vector(io, input_ct);
    // auto mac_input_ct = preprocess_vec(mac_M_vec.data(), data, *encryptor_, *encoder_);
    // send_ciphertext(io, input_ct);
    // send_ciphertext(io, mac_input_ct);

    if (verbose)
      std::cout << "[Client] Vector processed and sent" << std::endl;

    // 接收到server计算的输出并解密
    vector<Ciphertext> linear(ct_num);
    vector<Ciphertext> linear_mac(ct_num);
    vector<Ciphertext> mac_ver_ip(ct_num);
    auto start_time = clock_start();
    recv_encrypted_vector(io, context, linear);
    recv_encrypted_vector(io, context, linear_mac);
    recv_encrypted_vector(io, context, mac_ver_ip);
    wait_time += time_from(start_time);
    if (verbose)
      std::cout << "[Client] Receive ciphertexts of shares" << std::endl;
    // 解密函数返回的是地址
    vector<uint64_t*> linear_1(ct_num);
    vector<uint64_t*> linear_mac_1(ct_num);
    vector<uint64_t*> mac_ver_ip_1_addr(ct_num);
    for(uint64_t i=0; i<ct_num; i++){
      if(i<ct_num-1){
        linear_1[i] = fc_postprocess_mac(linear[i], data_1, *encoder_, *decryptor_);
        linear_mac_1[i] = fc_postprocess_mac(linear_mac[i], data_1, *encoder_, *decryptor_);
        mac_ver_ip_1_addr[i] = fc_postprocess_mac(mac_ver_ip[i], data_1, *encoder_, *decryptor_);
      }else{
        linear_1[i] = fc_postprocess_mac(linear[i], data, *encoder_, *decryptor_);
        linear_mac_1[i] = fc_postprocess_mac(linear_mac[i], data, *encoder_, *decryptor_);
        mac_ver_ip_1_addr[i] = fc_postprocess_mac(mac_ver_ip[i], data, *encoder_, *decryptor_);
      }
    }
    if (verbose)
      std::cout << "[Client] Obtain shares" << std::endl;

    // 得到输出结果
    vector<vector<uint64_t>> mac_ver_ip_1(ct_num, vector<uint64_t>(slot_count, 0));
    for(uint64_t i=0; i<ct_num; i++){
      if(i<ct_num-1){
        for(uint64_t j=0; j<slot_count; j++){
          output[i*slot_count+j] = linear_1[i][j];
          output_mac[i*slot_count+j] = linear_mac_1[i][j];
          mac_ver_ip_1[i][j] = mac_ver_ip_1_addr[i][j];
        }
      }else{
        mac_ver_ip_1[i].resize(remain);
        for(uint64_t j=0; j<remain; j++){
          output[i*slot_count+j] = linear_1[i][j];
          output_mac[i*slot_count+j] = linear_mac_1[i][j];
          mac_ver_ip_1[i][j] = mac_ver_ip_1_addr[i][j];
        }
      }
    }

    // Verify：输入没有被篡改
    vector<uint64_t> verify_msg(common_dim);
    for(uint64_t i=0; i<ct_num; i++){
      if(i<ct_num-1){
        for(uint64_t j=0; j<slot_count; j++){
          verify_msg[i*slot_count+j] = (mac_M_vec[i][j] + (prime_mod - mac_ver_ip_1[i][j])) % prime_mod;
        }
      }else{
        for(uint64_t j=0; j<remain; j++){
          verify_msg[i*slot_count+j] = (mac_M_vec[i][j] + (prime_mod - mac_ver_ip_1[i][j])) % prime_mod;
        }
      }
    }
    if (verbose)
      std::cout << "[Client] Verify message Generated" << std::endl;
    io->send_data(verify_msg.data(), sizeof(uint64_t) * common_dim);
    
    if(verify_output) { // 结果正确性校验
      // 一维数组存储在本地，便于遍历
      vector<uint64_t> input_vec_local(common_dim);
      vector<uint64_t> mac_M_local(common_dim);
      for (uint64_t i = 0; i < common_dim; i++) {
        input_vec_local[i] = input[i][0];
        mac_M_local[i] = mac_M[i][0];
      }

      io->send_data(input_vec_local.data(), sizeof(uint64_t) * common_dim);
      io->send_data(output, sizeof(uint64_t) * common_dim);
      io->send_data(output_mac, sizeof(uint64_t) * common_dim);
    }
  } else {
    // compute plaintext of delta
    vector<uint64_t> mac_vec(encoder_->slot_count(), mac_key);
    Plaintext* enc_mac = new Plaintext();
    encoder_->encode(mac_vec, *enc_mac);

    //Generate random_shares
    PRG prg;
    vector<vector<uint64_t>> op_shares_vec(ct_num, vector<uint64_t>(slot_count, 0));
    vector<vector<uint64_t>> mac_op_shares_vec(ct_num, vector<uint64_t>(slot_count, 0));
    vector<vector<uint64_t>> mac_ver_shares_vec(ct_num, vector<uint64_t>(slot_count, 0));
    vector<Plaintext> linear_0(ct_num);
    vector<Plaintext> linear_mac_0(ct_num);
    vector<Plaintext> mac_ver_shares_0(ct_num);
    for(uint64_t i=0; i<ct_num; i++){
      if(i<ct_num-1){
        if(!ss_zero){
          random_mod_p(prg, op_shares_vec[i].data(), slot_count, prime_mod);
          random_mod_p(prg, mac_op_shares_vec[i].data(), slot_count, prime_mod);
          random_mod_p(prg, mac_ver_shares_vec[i].data(), slot_count, prime_mod);
        }
        linear_0[i] = preprocess_vec_plain(op_shares_vec[i].data(), data_1, *encoder_);
        linear_mac_0[i] = preprocess_vec_plain(mac_op_shares_vec[i].data(), data_1, *encoder_);
        mac_ver_shares_0[i] = preprocess_vec_plain(mac_ver_shares_vec[i].data(), data_1, *encoder_);

        memcpy(output+i*slot_count, op_shares_vec[i].data(), slot_count*sizeof(uint64_t));
        memcpy(output_mac+i*slot_count, mac_op_shares_vec[i].data(), slot_count*sizeof(uint64_t));
      }else{
        op_shares_vec[i].resize(remain);
        mac_op_shares_vec[i].resize(remain);
        mac_ver_shares_vec[i].resize(remain);
        if(!ss_zero){
          random_mod_p(prg, op_shares_vec[i].data(), remain, prime_mod);
          random_mod_p(prg, mac_op_shares_vec[i].data(), remain, prime_mod);
          random_mod_p(prg, mac_ver_shares_vec[i].data(), remain, prime_mod);
        }
        linear_0[i] = preprocess_vec_plain(op_shares_vec[i].data(), data, *encoder_);
        linear_mac_0[i] = preprocess_vec_plain(mac_op_shares_vec[i].data(), data, *encoder_);
        mac_ver_shares_0[i] = preprocess_vec_plain(mac_ver_shares_vec[i].data(), data, *encoder_);

        memcpy(output+i*slot_count, op_shares_vec[i].data(), remain*sizeof(uint64_t));
        memcpy(output_mac+i*slot_count, mac_op_shares_vec[i].data(), remain*sizeof(uint64_t));
      }
    }
    
    if (verbose)
      cout << "[Server] Generate shares" << endl;

    //Receive ciphertext from client
    vector<Ciphertext> input_ct(ct_num);
    auto start_time = clock_start();
    recv_encrypted_vector(io, context, input_ct);
    wait_time += time_from(start_time);
    // Ciphertext mac_input_ct;
    // recv_ciphertext(io, context, input_ct_0);
    // recv_ciphertext(io, context, mac_input_ct);
    if (verbose)
      cout << "[Server] Receive Ciphertext from client" << endl;

    vector<Ciphertext> linear(ct_num);
    vector<Ciphertext> linear_mac(ct_num);
    vector<Ciphertext> mac_ver_op(ct_num);
    for(uint64_t i=0; i<ct_num; i++){

      // mux is either 0 or 1
      linear[i] = (mux==1) ? input_ct[i] : *zero;
      evaluator_->multiply_plain(linear[i], *enc_mac, linear_mac[i]);
      evaluator_->multiply_plain(input_ct[i], *enc_mac, mac_ver_op[i]);

      //Linear Share
      evaluator_->sub_plain_inplace(linear[i], linear_0[i]);
      //Linear MAC Share
      evaluator_->sub_plain_inplace(linear_mac[i], linear_mac_0[i]);
      //MAC Verification Share
      evaluator->sub_plain_inplace(mac_ver_op[i], mac_ver_shares_0[i]);
    }

    if (verbose)
      cout << "[Server] Generate Client shares" << endl;

    //Send ciphertexts to client
    send_encrypted_vector(io, linear);
    send_encrypted_vector(io, linear_mac);
    send_encrypted_vector(io, mac_ver_op);
    if (verbose)
      cout << "[Server] Send ciphertexts of shares to client" << endl;

    // Verify
    vector<uint64_t> mac_K_local(common_dim);
    for (uint64_t i = 0; i < common_dim; i++) {
      mac_K_local[i] = mac_K[i][0];
    }
    vector<uint64_t> verify_msg(common_dim);
    start_time = clock_start();
    io->recv_data(verify_msg.data(), sizeof(uint64_t)*common_dim);
    wait_time += time_from(start_time);
    uint64_t ctr = 0;
    for (uint64_t i = 0; i < common_dim; i++){
      uint64_t t1 = i/slot_count;
      uint64_t t2 = i%slot_count;
      if((mac_K_local[i] + mac_ver_shares_vec[t1][t2]) % prime_mod == verify_msg[i])
        ctr++;
      // std::cout << "verify_msg[i]:" << verify_msg[i] << std::endl;
    }
    if (verbose) cout << "Correct input Shares: " << ctr << endl;
    if(ctr < common_dim) cout << "Abort!"  << endl;

    if(verify_output) {
      vector<uint64_t> input_vec(common_dim);
      //receive client input
      io->recv_data(input_vec.data(), sizeof(uint64_t)*common_dim);

      vector<uint64_t> result_actual_output(common_dim, 0);
      vector<uint64_t> result_actual_mac(common_dim, 0);
      if(mux == 1){
        for(uint64_t i=0; i<common_dim; i++){
          result_actual_output[i] = input_vec[i];
          result_actual_mac[i] = mod_mult(result_actual_output[i], mac_key, prime_mod);
        }
      }

      // receive client_shares
      vector<uint64_t> op_shares_vec_1(common_dim);
      vector<uint64_t> mac_op_shares_vec_1(common_dim);
      io->recv_data(op_shares_vec_1.data(), sizeof(uint64_t)*common_dim);
      io->recv_data(mac_op_shares_vec_1.data(), sizeof(uint64_t)*common_dim);

      // reconstruct output
      for(uint64_t i=0; i<common_dim; i++) {
        op_shares_vec_1[i] = (op_shares_vec[i/slot_count][i%slot_count] + op_shares_vec_1[i])%prime_mod;
        mac_op_shares_vec_1[i] = (mac_op_shares_vec[i/slot_count][i%slot_count] + mac_op_shares_vec_1[i])%prime_mod;
      }

      //check for equality
      uint64_t mctr=0;
      uint64_t mvctr=0;
      for(uint64_t i=0;i<common_dim; i++) {
        if(result_actual_output[i] == op_shares_vec_1[i])
          mctr++;
        if(result_actual_mac[i] == mac_op_shares_vec_1[i])
          mvctr++;
      }
      if (verbose) cout<<"Correct Computation: "<< mctr <<endl;
      if (verbose) cout<<"Correct Mac Shares: "<< mvctr <<endl;
      if(mctr < common_dim) cout << "Computation error! Abort!"  << endl;
      if(mvctr < common_dim) cout << "Mac error! Abort!"  << endl;
    }
  }
}

/*
void FCField::matrix_multiplication(int32_t num_rows, int32_t common_dim,
                                    int32_t num_cols,
                                    vector<vector<uint64_t>> &A,
                                    vector<vector<uint64_t>> &B,
                                    vector<vector<uint64_t>> &C,
                                    bool verify_output, bool verbose) {
  assert(num_cols == 1);
  data.filter_h = num_rows;
  data.filter_w = common_dim;
  data.image_size = common_dim;
  this->slot_count =
      min(max(8192, 2 * next_pow2(common_dim)), SEAL_POLY_MOD_DEGREE_MAX);
  configure();

  Encryptor *encryptor_;
  Decryptor *decryptor_;
  Evaluator *evaluator_;
  BatchEncoder *encoder_;
  GaloisKeys *gal_keys_;
  Ciphertext *zero_;
  if (slot_count > POLY_MOD_DEGREE) {
    generate_new_keys(party, io, slot_count, context, encryptor_, decryptor_,
                      evaluator_, encoder_, gal_keys_, zero_);
  } else {
    encryptor_ = this->encryptor;
    decryptor_ = this->decryptor;
    evaluator_ = this->evaluator;
    encoder_ = this->encoder;
    gal_keys_ = this->gal_keys;
    zero_ = this->zero;
  }

  if (party == BOB) {
    vector<uint64_t> vec(common_dim);
    for (uint64_t i = 0; i < common_dim; i++) {
      vec[i] = B[i][0];
    }
    if (verbose)
      cout << "[Client] Vector Generated" << endl;

    auto ct = preprocess_vec(vec.data(), data, *encryptor_, *encoder_);
    send_ciphertext(io, ct);
    if (verbose)
      cout << "[Client] Vector processed and sent" << endl;

    Ciphertext enc_result;
    recv_ciphertext(io, context, enc_result);
    auto HE_result = fc_postprocess(enc_result, data, *encoder_, *decryptor_);
    if (verbose)
      cout << "[Client] Result received and decrypted" << endl;

    for (uint64_t i = 0; i < num_rows; i++) {
      C[i][0] = HE_result[i];
    }
    if (verify_output)
      verify(&vec, nullptr, C);

    delete[] HE_result;
  } else // party == ALICE
  {
    vector<uint64_t> vec(common_dim);
    for (uint64_t i = 0; i < common_dim; i++) {
      vec[i] = B[i][0];
    }
    if (verbose)
      cout << "[Server] Vector Generated" << endl;
    vector<uint64_t *> matrix_mod_p(num_rows);
    vector<uint64_t *> matrix(num_rows);
    for (uint64_t i = 0; i < num_rows; i++) {
      matrix_mod_p[i] = new uint64_t[common_dim];
      matrix[i] = new uint64_t[common_dim];
      for (uint64_t j = 0; j < common_dim; j++) {
        matrix_mod_p[i][j] = neg_mod((int64_t)A[i][j], (int64_t)prime_mod);
        matrix[i][j] = A[i][j];
      }
    }
    if (verbose)
      cout << "[Server] Matrix generated" << endl;

    PRG prg;
    uint64_t *secret_share = new uint64_t[num_rows];
    random_mod_p(prg, secret_share, num_rows, prime_mod);

    Ciphertext enc_noise =
        fc_preprocess_noise(secret_share, data, *encryptor_, *encoder_);
    auto encoded_mat = preprocess_matrix(matrix_mod_p.data(), data, *encoder_);
    if (verbose)
      cout << "[Server] Matrix and noise processed" << endl;

    Ciphertext ct;
    recv_ciphertext(io, context, ct);

#ifdef HE_DEBUG
    PRINT_NOISE_BUDGET(decryptor_, ct, "before FC Online");
#endif

    auto HE_result = fc_online(ct, encoded_mat, data, *evaluator_, *gal_keys_,
                               *zero_, enc_noise);
/*
#ifdef HE_DEBUG
    PRINT_NOISE_BUDGET(decryptor_, HE_result, "after FC Online");
#endif

    parms_id_type parms_id = HE_result.parms_id();
    shared_ptr<const SEALContext::ContextData> context_data =
        context->get_context_data(parms_id);
    flood_ciphertext(HE_result, context_data, SMUDGING_BITLEN);

#ifdef HE_DEBUG
    PRINT_NOISE_BUDGET(decryptor_, HE_result, "after noise flooding");
#endif*/

    //evaluator_->mod_switch_to_next_inplace(HE_result);
/*
#ifdef HE_DEBUG
    PRINT_NOISE_BUDGET(decryptor_, HE_result, "after mod-switch");
#endif

    send_ciphertext(io, HE_result);
    if (verbose)
      cout << "[Server] Result computed and sent" << endl;

    auto result = ideal_functionality(vec.data(), matrix.data());

    for (uint64_t i = 0; i < num_rows; i++) {
      C[i][0] = neg_mod((int64_t)result[i] - (int64_t)secret_share[i],
                        (int64_t)prime_mod);
    }
    if (verify_output)
      verify(&vec, &matrix, C);

    for (uint64_t i = 0; i < num_rows; i++) {
      delete[] matrix_mod_p[i];
      delete[] matrix[i];
    }
    delete[] secret_share;
  }
  if (slot_count > POLY_MOD_DEGREE) {
    free_keys(party, encryptor_, decryptor_, evaluator_, encoder_, gal_keys_,
              zero_);
  }
}
*/
void FCField::verify(vector<uint64_t> *vec, vector<uint64_t *> *matrix,
                     vector<vector<uint64_t>> &C) {
  if (party == BOB) {
    io->send_data(vec->data(), data.filter_w * sizeof(uint64_t));
    io->flush();
    for (uint64_t i = 0; i < data.filter_h; i++) {
      io->send_data(C[i].data(), sizeof(uint64_t));
    }
  } else // party == ALICE
  {
    vector<uint64_t> vec_0(data.filter_w);
    io->recv_data(vec_0.data(), data.filter_w * sizeof(uint64_t));
    for (uint64_t i = 0; i < data.filter_w; i++) {
      vec_0[i] = (vec_0[i] + (*vec)[i]) % prime_mod;
    }/*
    auto result = ideal_functionality(vec_0.data(), matrix->data());

    vector<vector<uint64_t>> C_0(data.filter_h);
    for (uint64_t i = 0; i < data.filter_h; i++) {
      C_0[i].resize(1);
      io->recv_data(C_0[i].data(), sizeof(uint64_t));
      C_0[i][0] = (C_0[i][0] + C[i][0]) % prime_mod;
    }
    bool pass = true;
    for (uint64_t i = 0; i < data.filter_h; i++) {
      if (neg_mod(result[i], (int64_t)prime_mod) != (int64_t)C_0[i][0]) {
        pass = false;
      }
    }
    if (pass)
      cout << GREEN << "[Server] Successful Operation" << RESET << endl;
    else {
      cout << RED << "[Server] Failed Operation" << RESET << endl;
      cout << RED << "WARNING: The implementation assumes that the computation"
           << endl;
      cout << "performed locally by the server (on the model and its input "
              "share)"
           << endl;
      cout << "fits in a 64-bit integer. The failed operation could be a result"
           << endl;
      cout << "of overflowing the bound." << RESET << endl;
    }*/
  }
}
