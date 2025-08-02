#include "emp-tool/emp-tool.h"
#include "emp-zk/emp-zk.h"
#include <iostream>

#define ZK_F 16
#define ZK_INT_LEN 62

using namespace emp;
using namespace std;

int port, party;
const int threads = 1;

int size = 10000;

uint64_t comm(BoolIO<NetIO> *ios[threads])
{
	uint64_t c = 0;
	for (int i = 0; i < threads; ++i)
		c += ios[i]->counter;
	return c;
}

void test_gelu(BoolIO<NetIO> *ios[threads], int party, IntFp *x, IntFp *y, int dim)
{
	// // step 1: CMP + compute absolute value of x
	// Float zero(0., PUBLIC);
	// vector<Integer> x_bool(dim);
	// arith2bool<BoolIO<NetIO>>(x_bool.data(), x, dim);
	// //
	// vector<Integer> negx_bool(dim);
	// IntFp *negx = new IntFp[dim];
	// for (int i = 0; i < dim; i++){
	// 	negx[i] = x[i] * (PR-1);
	// }
	// arith2bool<BoolIO<NetIO>>(negx_bool.data(), negx, dim);
	// //
	// vector<Float> x_float(dim, (0.0, PUBLIC));
	// vector<Float> negx_float(dim, (0.0, PUBLIC));
	// Float *abx_float = new Float[dim];
	// for (int i = 0; i < dim; i++){
	// 	x_float[i] = Int62ToFloat(x_bool[i], ZK_F);
	// 	negx_float[i] = Int62ToFloat(negx_bool[i], ZK_F);
	// 	abx_float[i] = If(x_float[i].less_than(zero), negx_float[i], x_float[i]);
	// }

	// // step 2: compute abx^3 and then truncation
	// Integer *abx_bool = new Integer[dim];
	// IntFp *abx = new IntFp[dim];
	// for (int i = 0; i , dim; i++){
	// 	abx_bool[i] = FloatToInt62(abx_float[i], ZK_F);
	// }
	// bool2arith<BoolIO<NetIO>>(abx, abx_bool, dim);

	// step 1: compute x^3 and then truncation
	IntFp *k = new IntFp[dim];
	for (int i = 0; i < dim; i++){
		k[i] = x[i] * x[i] * x[i];
	}
	vector<Integer> k_bool(dim);
	arith2bool<BoolIO<NetIO>>(k_bool.data(), k, dim);
	for (size_t i = 0; i < k_bool.size(); i++){
		Float fl = std::move(Int62ToFloat(k_bool[i], 3 * ZK_F));
		k_bool[i] = FloatToInt62(fl, ZK_F);
	}
	bool2arith<BoolIO<NetIO>>(k, k_bool.data(), dim);

	// step 3: compute t and truncation
	IntFp *t = new IntFp[dim];
	int64_t a_scale = floor(0.044715 * (1ULL << ZK_F));
    uint64_t a = a_scale < 0 ? PR + a_scale : a_scale; 
	int64_t b_scale = floor(sqrt(2/3.14159265358979323846) * (1ULL << ZK_F));
    uint64_t b = b_scale < 0 ? PR + b_scale : b_scale; 
	for (int i = 0; i < dim; i++){
		t[i] = (x[i] + k[i] * a) * b;
	}
	vector<Integer> t_bool(dim);
	arith2bool<BoolIO<NetIO>>(t_bool.data(), t, dim);
	for (size_t i = 0; i < t_bool.size(); i++){
		Float fl = std::move(Int62ToFloat(t_bool[i], 3 * ZK_F));
		t_bool[i] = FloatToInt62(fl, ZK_F);
	}

	// step 4: compute exp and div, to obtain tanh
	Float ir_res1(0., PUBLIC);
	Float ir_res2(0., PUBLIC);
	Float negone((PR-1), PUBLIC);
	Float one(1., PUBLIC);
	Float c1(0., PUBLIC);
	Float c2(0., PUBLIC);
	Float ir_res(0., PUBLIC);
	Integer *div_out_bool = new Integer[dim];
	Integer *c1_out_bool = new Integer[dim];
	for (auto i = 0; i < dim; i++){
		ir_res1 = Int62ToFloat(t_bool[i], ZK_F); 
		ir_res2 = ir_res1 * negone;
		ir_res1 = ir_res1.exp();	
		ir_res2 = ir_res2.exp();

		c1 = ir_res1 - ir_res2;
		c2 = ir_res1 + ir_res2;

		ir_res = (one / c2);
		div_out_bool[i] = FloatToInt62(ir_res, ZK_F);
		c1_out_bool[i] = FloatToInt62(c1, ZK_F);
	}

	IntFp *div_out = new IntFp[dim];
	IntFp *c1_out = new IntFp[dim];
	bool2arith<BoolIO<NetIO>>(div_out, div_out_bool, dim);
	bool2arith<BoolIO<NetIO>>(c1_out, c1_out_bool, dim);
	IntFp *h = new IntFp[dim];
	for (int i = 0; i < dim; i++){
		h[i] = c1_out[i] * div_out[i];
	}
	vector<Integer> h_bool(dim);
	arith2bool<BoolIO<NetIO>>(h_bool.data(), h, dim);
	for (size_t i = 0; i < h_bool.size(); i++){
		Float fl = std::move(Int62ToFloat(h_bool[i], 2 * ZK_F));
		h_bool[i] = FloatToInt62(fl, ZK_F);
	}
	bool2arith<BoolIO<NetIO>>(h, h_bool.data(), dim);

	// step 5: compute gelu
	int64_t c_scale = floor(0.5 * (1ULL << ZK_F));
    uint64_t c = c_scale < 0 ? PR + c_scale : c_scale; 
	for (int i = 0; i < dim; i++){
		y[i] = x[i] * c * (h[i] + 1);
	}
	vector<Integer> y_bool(dim);
	arith2bool<BoolIO<NetIO>>(y_bool.data(), y, dim);
	for (size_t i = 0; i < y_bool.size(); i++){
		Float fl = std::move(Int62ToFloat(y_bool[i], 3 * ZK_F));
		y_bool[i] = FloatToInt62(fl, ZK_F);
	}
	bool2arith<BoolIO<NetIO>>(y, y_bool.data(), dim);

}

int main(int argc, char **argv)
{
	parse_party_and_port(argv, &party, &port);
	BoolIO<NetIO> *ios[threads];
	for (int i = 0; i < threads; ++i)
		ios[i] = new BoolIO<NetIO>(new NetIO(party == ALICE ? nullptr : "127.0.0.1", port + i), party == ALICE);

	std::cout << std::endl
			  << "------------ Mystique - gelu zero-knowledge proof test ------------" << std::endl
			  << std::endl;

	setup_zk_bool<BoolIO<NetIO>>(ios, threads, party);
	setup_zk_arith<BoolIO<NetIO>>(ios, threads, party, true);

	sync_zk_bool<BoolIO<NetIO>>();

	cout << "start data generation" << endl;

	srand(time(NULL));

	uint64_t *a = new uint64_t[size];
	for (int i = 0; i < size; ++i)
	{
		// cout << i << endl;
		a[i] = rand() % PR;
	}

	IntFp *a_zks = new IntFp[size];
	IntFp *output = new IntFp[size];
	batch_feed(a_zks, a, size);

	cout << "start test" << endl;

	uint64_t com1 = comm(ios);
	auto start = clock_start();

	test_gelu(ios, party, a_zks, output, size);

	finalize_zk_bool<BoolIO<NetIO>>();
	finalize_zk_arith<BoolIO<NetIO>>();

	double time1 = time_from(start);
	cout << "time - gelu: " << time1 / 1000 << " ms\t " << party << endl;
	uint64_t com11 = comm(ios) - com1;
	std::cout << "communication - gelu (KB): " << com11 / 1024.0 << std::endl;

	cout << "finish test" << endl;

	for (int i = 0; i < threads; ++i)
	{
		delete ios[i]->io;
		delete ios[i];
	}
	return 0;
}
