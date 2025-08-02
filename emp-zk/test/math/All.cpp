#include "emp-tool/emp-tool.h"
#include "emp-zk/emp-zk.h"
#include <iostream>

using namespace emp;
using namespace std;

int port, party;
const int threads = 1;

int dim = 10000;
int rows = 250, cols = 40;

uint64_t comm(BoolIO<NetIO> *ios[threads])
{
	uint64_t c = 0;
	for (int i = 0; i < threads; ++i)
		c += ios[i]->counter;
	return c;
}

// TODO: 需要实现
void gelu_extractBits(IntFp *x, IntFp *y, int h, int s, int dim) {}

void ZKPositiveDigDec(IntFp *x, IntFp *y, uint64_t *digit_size, int num_digits, int dim)  // x:dim；y:num_digits*dim; num_digits:子串数，digit_size:每个子串的比特长度 digit_size:num_digits
{
	uint64_t ori_y = 0;
	IntFp *sum_ydigits = new IntFp[dim];  // 生成sum_ydigits是为了进行Prove时使用
	vector<IntFp> zero;

	// step1: compute y + check range
	for (int i = 0; i < dim; i++)
	{
		int shiftlen = 0;
		uint64_t curr_digitlen = digit_size[0];
		uint64_t digit_mask = (1ULL << curr_digitlen) - 1;

		uint64_t curr_x;
		if (party == ALICE){
			curr_x = (uint64_t)HIGH64(x[i].value);
			ori_y = (curr_x >> shiftlen) & digit_mask; // 此处先放了低位子串
		}

		sum_ydigits[i] = IntFp(ori_y, ALICE);
		y[i * num_digits] = sum_ydigits[i]; 

		// check range
		if (curr_digitlen > 1 && curr_digitlen < NUM_RANGE){
			LUTRange[curr_digitlen]->LUTRangeread(y[i * num_digits]);
		} else if (curr_digitlen == 1){
			IntFp r = (y[i * num_digits].negate() + 1) * y[i * num_digits];
			zero.push_back(r);
		} else {
			error("Incorrect decomposition length");
		}

		for (int j = 1; j < num_digits; j++)
		{
			curr_digitlen = digit_size[j];

			digit_mask = (1ULL << curr_digitlen) - 1;
			shiftlen += digit_size[j - 1];

			if (party == ALICE){
				ori_y = (curr_x >> shiftlen) & digit_mask; // 此处先放了低位子串
			}

			IntFp y_digit = IntFp(ori_y, ALICE);
			y[i * num_digits + j] = y_digit; 

			sum_ydigits[i] = sum_ydigits[i] + y_digit * (1ULL << shiftlen);

			// check range
			if (curr_digitlen > 1 && curr_digitlen < NUM_RANGE){
				LUTRange[curr_digitlen]->LUTRangeread(y_digit);
			} else if (curr_digitlen == 1){
				IntFp r = (y_digit.negate() + 1) * y_digit;
				zero.push_back(r);
			} else {
				error("Incorrect decomposition length");
			}
		}

		// sum_ydigits[i] = x[i] + sum_ydigits[i].negate(); // 重用它，放置了checkzero的输入
		zero.push_back(x[i] + sum_ydigits[i].negate());
	}

	// cout << "ZKPositiveDigDec -- step 2" << endl;
	// step2: check zero
	// bool res = batch_reveal_check_zero(sum_ydigits, dim);
	bool res = batch_reveal_check_zero(zero.data(), zero.size());
	if (!res)
		error("step 2: batch_reveal_check_zero failed");

	delete[] sum_ydigits;
}

void ZKPositiveDigDecAny(IntFp *x, IntFp *xhigh, IntFp *xlow, uint64_t highsize, uint64_t lowsize, int dim)    // 分解成任意长度 x: dim; xhigh: dim; ylow: dim
{
	uint64_t k = ceil(log2(PR));
	assert(highsize + lowsize < k);

	// step 1: 处理分解长度到合适
	uint64_t high_num_digit = ceil((double)highsize/(NUM_RANGE - 1));
	uint64_t most_highsize = highsize - (high_num_digit - 1) * (NUM_RANGE - 1);
	// cout << "highsize = " << highsize << endl;
	// cout << "high_num_digit = " << high_num_digit << endl;
	// cout << "most_highsize = " << most_highsize << endl;
	
	uint64_t low_num_digit = ceil((double)lowsize/(NUM_RANGE - 1));
	uint64_t most_lowsize = lowsize - (low_num_digit - 1) * (NUM_RANGE - 1);
	// cout << "lowsize = " << lowsize << endl;
	// cout << "low_num_digit = " << low_num_digit << endl;
	// cout << "most_lowsize = " << most_lowsize << endl;

	// step 2: 构造uint64_t *digit_size和int num_digits
	// cout << "@@@@@@@ begin step 2" << endl;
	uint64_t num_digits = high_num_digit + low_num_digit;
	uint64_t *digit_size = new uint64_t[num_digits];
	for (int i = 0; i < low_num_digit - 1; i++){
		digit_size[i] = NUM_RANGE - 1;
	}
	digit_size[low_num_digit - 1] = most_lowsize;
	for (int i = low_num_digit; i < num_digits - 1; i++){
		digit_size[i] = NUM_RANGE - 1;
	}
	digit_size[num_digits - 1] = most_highsize;

	// step 3: invoke ZKPositiveDigDec()
	// cout << "@@@@@@@ begin step 3" << endl;
	IntFp *y_digits = new IntFp[dim * num_digits];
	ZKPositiveDigDec(x, y_digits, digit_size, num_digits, dim);

	// step 4: construct y
	// cout << "@@@@@@@ begin step 4" << endl;
	for (int i = 0; i < dim; i++){
		// -- 先处理低位
		int shiftlen = 0;
		xlow[i] = y_digits[i * num_digits];
		for (int j = 1; j < low_num_digit; j++){
			shiftlen += digit_size[j - 1];
			xlow[i] = xlow[i] + y_digits[i * num_digits + j] * (1ULL << shiftlen);
		}

		// -- 再处理高位
		shiftlen = 0;
		xhigh[i] = y_digits[i * num_digits + low_num_digit];
		for (int j = 1; j < high_num_digit; j++){
			shiftlen += digit_size[low_num_digit + j - 1];
			xhigh[i] = xhigh[i] + y_digits[i * num_digits + low_num_digit + j] * (1ULL << shiftlen);
		}
	}
	// cout << "@@@@@@@ end ZKPositiveDigDecAny" << endl;
	delete[] digit_size;
	delete[] y_digits;
}

void ZKpositiveTrunc12(IntFp *x, IntFp *y, int dim, uint64_t trunclen = 12)  // 所有输入截断相同的长度trunclen，必须是12比特
{
	uint64_t ori_y = 0;
	uint64_t digit = 0;
	IntFp *sum_digit = new IntFp[dim]; // 生成sum_ydigits是为了进行Prove时使用

	int64_t digit_mask = (1LL << trunclen) - 1;
	int64_t y_mask = (1LL << (BIT_LENGTH - trunclen)) - 1;

	for (int i = 0; i < dim; i++)
	{
		int shiftlen = 0;

        // step 1: obtain y
		uint64_t curr_x;
		if (party == ALICE)
		{
			curr_x = (uint64_t)HIGH64(x[i].value);
            ori_y = (curr_x >> trunclen) & y_mask;

			digit = (curr_x >> shiftlen) & digit_mask;  // for check
		}

		y[i] = IntFp(ori_y, ALICE);

		sum_digit[i] = IntFp(digit, ALICE);
		if (trunclen == 12) {
			LUTRange[12]->LUTRangeread(sum_digit[i]);
		} else {
			error("Incorrect decomposition length");
		}
		
        // step 2: check range
		for (int j = 1; j < BIT_LENGTH/trunclen; j++)  // 两个int类型向下取整a,b就是直接a/b
		{
			// BIT_LENGTH/trunclen 除不尽，会多出1。这是没有关系的，因为此处的functionality是positiveTrunc，最高的1位就是0.
			// cout << "digit number = " << BIT_LENGTH/trunclen << endl;

			shiftlen += trunclen;
			if (party == ALICE){
				digit = (curr_x >> shiftlen) & digit_mask;
			}

            IntFp mdigit = IntFp(digit, ALICE);
			if (trunclen == 12) {
				LUTRange[12]->LUTRangeread(mdigit);
			} else {
				error("Incorrect decomposition length");
			}

			sum_digit[i] = mdigit * (1LL << shiftlen) + sum_digit[i];
		}

		sum_digit[i] = x[i] + sum_digit[i].negate(); // 重用它，放置了checkzero的输入
	}

	// step2: check zero
	bool res = batch_reveal_check_zero(sum_digit, dim);
	if (!res)
		error("step 2: batch_reveal_check_zero failed");

	delete[] sum_digit;
}

void ZKpositiveTruncAny(IntFp *x, IntFp *y, int dim, uint64_t trunclen)   // 截断任意长度
{
	uint64_t m = ceil(log2(PR)) - 1;
	IntFp *ylow = new IntFp[dim];
	ZKPositiveDigDecAny(x, y, ylow, m - trunclen, trunclen, dim);

	delete[] ylow;
}

void ZKCompareShift(IntFp *x, IntFp *y, int shiftlen, int dim)    // funcationality: 与2^{ceil(logPR) - 1}比较大小，可以通过简单的右移实现；
{
	Integer *x_bool = new Integer[dim];
	Integer *y_bool = new Integer[dim];
	arith2bool<BoolIO<NetIO>>(x_bool, x, dim);
	for (int i = 0; i < dim; i++){
		y_bool[i] = x_bool[i] >> shiftlen;
	}
	bool2arith<BoolIO<NetIO>>(y, y_bool, dim);

	delete[] x_bool;
	delete[] y_bool;
}

void ZKCompareConstant(IntFp *x, uint64_t y, IntFp *z, int dim)   // if <= output 1; 比ZKCompareShift更复杂的比较，即通过简单的右移无法实现
{
	Integer *x_bool = new Integer[dim];
	Integer *z_bool = new Integer[dim];
	arith2bool<BoolIO<NetIO>>(x_bool, x, dim);

	// cout << "x_bool.size = " << x_bool[0].size() << endl;
	Integer y_bool = Integer(x_bool[0].size(), y + 1, PUBLIC);  // 将<=转化为<，warning: 条件是y+1不溢出
	// Integer one = Integer(1, PUBLIC);

	for (int i = 0; i < dim; i++){
		Integer res = x_bool[i] - y_bool;
		z_bool[i] = res >> BIT_LENGTH;
	}
	bool2arith<BoolIO<NetIO>>(z, z_bool, dim);

	delete[] x_bool;
	delete[] z_bool;
}

void ZKFpCompare(IntFp *x, IntFp *y, IntFp *z, int dim)    // if x < y output 1 两个MAC值的比较
{
	Integer *x_bool = new Integer[dim];
	Integer *y_bool = new Integer[dim];
	Integer *z_bool = new Integer[dim];
	arith2bool<BoolIO<NetIO>>(x_bool, x, dim);    
	arith2bool<BoolIO<NetIO>>(y_bool, y, dim);
	
	for (int i = 0; i < dim; i++){
		Integer res = x_bool[i] - y_bool[i];
		z_bool[i] = res >> BIT_LENGTH;
	}
	bool2arith<BoolIO<NetIO>>(z, z_bool, dim);

	delete[] x_bool;
	delete[] y_bool;
	delete[] z_bool;
}

void ZKFpCompareLEQ(IntFp *x, IntFp *y, IntFp *z, int dim)    // if x <= y outout 1 两个MAC值的比较
{
	// step 1: 将<= 转化为 <
	IntFp *yaddone = new IntFp[dim];
	for (int i = 0; i < dim; i++){
		yaddone[i] = y[i] + 1;   // warning: y+1不溢出才可以
	}
	Integer *x_bool = new Integer[dim];
	Integer *y_bool = new Integer[dim];
	Integer *z_bool = new Integer[dim];
	arith2bool<BoolIO<NetIO>>(x_bool, x, dim);    
	arith2bool<BoolIO<NetIO>>(y_bool, yaddone, dim);
	
	for (int i = 0; i < dim; i++){
		Integer res = x_bool[i] - y_bool[i];
		z_bool[i] = res >> BIT_LENGTH;
	}
	bool2arith<BoolIO<NetIO>>(z, z_bool, dim);

	delete[] yaddone;
	delete[] x_bool;
	delete[] y_bool;
	delete[] z_bool;
}

void ZKFpCompareReal(IntFp *x, IntFp *y, IntFp *z, int dim)    // 
{}

void ZKMax(IntFp *x, IntFp *y, int rows, int cols)    // matrix中每一行求一个max; x: rows*cols  y:rows
{
	Integer *x_bool = new Integer[rows * cols];
	Integer *y_bool = new Integer[rows];
	arith2bool<BoolIO<NetIO>>(x_bool, x, rows * cols);       

	for (int i = 0; i < rows; i++){
		y_bool[i] = x_bool[i * cols];
		for (int j = 1; j < cols; j++){
			Integer curr = x_bool[i * cols + j];
			Bit res = curr.geq(y_bool[i]);
			y_bool[i] = y_bool[i].select(res, curr);
		}
	}
	bool2arith<BoolIO<NetIO>>(y, y_bool, rows);

	delete[] x_bool;
	delete[] y_bool;
}

void ZKgeneralDigDec(IntFp *x, IntFp *y, uint64_t *digit_size, int num_digits, int dim)
{
	// step 1: CMP and obtain b
	uint64_t shiftlen = BIT_LENGTH - 1;
	uint64_t a = 1ULL << shiftlen;
	IntFp *tmp = new IntFp[dim];
	IntFp *b = new IntFp[dim];
	ZKCompareShift(x, b, shiftlen, dim);

	// step 2: DigitDec
	for (int i = 0; i < dim; i++){
		tmp[i] = x[i] + (b[i] * a).negate();
	}
	// IntFp *digdec = new IntFp[dim * num_digits];
	digit_size[num_digits - 1] = digit_size[num_digits - 1] - 1;
	ZKPositiveDigDec(tmp, y, digit_size, num_digits, dim);

	// step 3: compute y_{k-1}
	for (int i = 0; i < dim; i++){
		y[i * num_digits + (num_digits - 1)] = y[i * num_digits + (num_digits - 1)] + b[i] * (1ULL << digit_size[num_digits - 1]);
	}

	delete[] tmp;
	delete[] b;
}

void ZKgeneralDigDecAny(IntFp *x, IntFp *y, uint64_t *digit_size, int num_digits, int dim)
{
	// step 1: CMP and obtain b
	uint64_t shiftlen = BIT_LENGTH - 1;
	IntFp *b = new IntFp[dim];
	ZKCompareShift(x, b, shiftlen, dim);

	// step 2: DigitDec
	uint64_t a = 1ULL << shiftlen;
	IntFp *beforeDigdec = new IntFp[dim];
	for (int i = 0; i < dim; i++){
		beforeDigdec[i] = x[i] + (b[i] * a).negate();
	}
	digit_size[num_digits - 1] = digit_size[num_digits - 1] - 1;

	// step 2-1: 生成输入到ZKPositiveDigDec中的digit_resize和renum_digits
	vector<uint64_t> digit_resize;
	vector<uint64_t> each_digit_resize_num;
	uint64_t renum_digits = 0;
	
	for(int i = 0; i < num_digits; i++){
		uint64_t curr_digit_size = digit_size[i];
		uint64_t num_digit = ceil((double)curr_digit_size/(NUM_RANGE - 1));
		uint64_t high_size = curr_digit_size - (num_digit - 1) * (NUM_RANGE - 1);
		for (int j = 0; j < num_digit - 1; j++){
			digit_resize.push_back(NUM_RANGE - 1);
		}
		digit_resize.push_back(high_size);
		each_digit_resize_num.push_back(num_digit);
		renum_digits += num_digit;
	}

	// step 2-2: invoke ZKPositiveDigDec
	IntFp *tmp_y = new IntFp[dim * renum_digits];
	ZKPositiveDigDec(beforeDigdec, tmp_y, digit_resize.data(), renum_digits, dim);

	// step 2-3: 合并输出到原始形态
	for (int i = 0; i < dim; i++){
		uint64_t start_digit_index = 0;
		for (int j = 0; j < num_digits; j++){

			// 初始化为(分解出的最低子串)
			int shiftlen = 0;
			y[i * num_digits + j] = tmp_y[i * renum_digits + start_digit_index];

			// 属于当前y子串的(分解出的所有子串)的数量
			uint64_t curr_redigits_num = each_digit_resize_num[j];

			// 累加属于当前y子串的(分解出的所有子串)
			for (int k = 1; k < curr_redigits_num; k++){
				IntFp curr_digit = tmp_y[i * renum_digits + start_digit_index + k];
				shiftlen += digit_resize[start_digit_index + k - 1];
				y[i * num_digits + j] = y[i * num_digits + j] + curr_digit * (1ULL << shiftlen);
			}

			// 为下一个y子串指明(分解出的最低子串)的index
			start_digit_index += curr_redigits_num;
		}

	// step 3: compute y_{k-1}
		y[i * num_digits + (num_digits - 1)] = y[i * num_digits + (num_digits - 1)] + b[i] * (1ULL << digit_size[num_digits - 1]);
	}

	// // step 3: compute y_{k-1}
	// for (int i = 0; i < dim; i++){
	// 	y[i * num_digits + (num_digits - 1)] = y[i * num_digits + (num_digits - 1)] + b[i] * (1ULL << digit_size[num_digits - 1]);
	// }

	delete[] b;
	delete[] beforeDigdec;
	delete[] tmp_y;
}

void ZKmsnzb(IntFp *x, IntFp *y, int dim)
{
	uint64_t msnzb = 0;
	uint64_t z = 0;
	IntFp *z1 = new IntFp[dim];
	IntFp *z2 = new IntFp[dim];

	for (int i = 0; i < dim; i++)
	{
		if (party == ALICE)
		{
			uint64_t curr_x = (uint64_t)HIGH64(x[i].value);
			msnzb = floor(log2(curr_x));   // msnzb: 最高非零位的index (从0开始)
			z = 1ULL << msnzb;
		}
		y[i] = IntFp(msnzb, ALICE);

		IntFp value = IntFp(z, ALICE);
        LUTmsnzb->LUTread(y[i], value);

		z1[i] = value;
		z2[i] = value * 2;
	}

    IntFp *b0 = new IntFp[dim];
	IntFp *b1 = new IntFp[dim];
	ZKFpCompareLEQ(z1, x, b0, dim);
	ZKFpCompare(x, z2, b1, dim);   // if x < y output 1

	for (int i = 0; i < dim; i++){
		// b0[i] = b0[i] * b1[i] + (PR - 1);
		b0[i] = b0[i] + (PR - 1);
		b1[i] = b1[i] + (PR - 1);
	}
	bool res = batch_reveal_check_zero(b0, dim);
	if (!res)
		error("batch_reveal_check_zero failed");
	res = batch_reveal_check_zero(b1, dim);
	if (!res)
		error("batch_reveal_check_zero failed");

	delete[] z1;
	delete[] z2;
	delete[] b0;
	delete[] b1;
}

void ZKgeneralTrunc12(IntFp *x, IntFp *y, int dim, uint64_t trunclen = SCALE)   // 只用到了LayerNorm trunc长度 = 12
{
	// step 1: cmp
	uint64_t a = (PR - 1)/2;
	IntFp *b = new IntFp[dim];
	ZKCompareConstant(x, a, b, dim);

	// step 2: line 2
	IntFp *z = new IntFp[dim];
	IntFp *zTrunc = new IntFp[dim];
	IntFp *t1 = new IntFp[dim];
	IntFp *t2 = new IntFp[dim];
	for (int i = 0; i < dim; i++){
		t1[i] = (b[i] * 2).negate() + 1;
		t2[i] = b[i].negate() + 1;
		z[i] = t1[i] * x[i] + t2[i].negate();
	}
	ZKpositiveTrunc12(z, zTrunc, dim);

	// step 3: line 3
	for (int i = 0; i < dim; i++){
		y[i] = t1[i] * (zTrunc[i] + t2[i]);
	}

	delete[] b;
	delete[] z;
	delete[] zTrunc;
	delete[] t1;
	delete[] t2;
}

void ZKgeneralTruncAny(IntFp *x, IntFp *y, int dim, uint64_t trunclen)   
{
	// step 1: cmp
	uint64_t a = (PR - 1)/2;
	IntFp *b = new IntFp[dim];
	ZKCompareConstant(x, a, b, dim);
	cout << "Trunc - step 1" << endl;

	// step 2: line 2
	IntFp *z = new IntFp[dim];
	IntFp *zTrunc = new IntFp[dim];
	IntFp *t1 = new IntFp[dim];
	IntFp *t2 = new IntFp[dim];
	for (int i = 0; i < dim; i++){
		t1[i] = b[i] * 2 + (PR -1);
		t2[i] = b[i].negate() + 1;
		z[i] = t1[i] * x[i] + t2[i].negate();
	}
	ZKpositiveTruncAny(z, zTrunc, dim, trunclen);
	cout << "Trunc - step 2" << endl;

	// step 3: line 3
	for (int i = 0; i < dim; i++){
		cout << "i = " << i << endl;
		y[i] = t1[i] * zTrunc[i] + t2[i].negate();
	}
	cout << "Trunc - step 3" << endl;

	delete[] b;
	delete[] z;
	delete[] zTrunc;
	delete[] t1;
	delete[] t2;
}

void ZKGeLU(BoolIO<NetIO> *ios[threads], int party, uint64_t dim, IntFp *x, IntFp *y)
{
	uint64_t com1 = comm(ios);
	auto start = clock_start();

	// step1: lines 1-4
	int alpha = 4;
	int alpha1 = alpha * (1 << SCALE);
	int beta = 2 * alpha1;
	int h = log2(beta);
	int s = 8;
	int len = 256;

	IntFp *x1 = new IntFp[dim];
	for (int i = 0; i < dim; i++)
	{
		x1[i] = x[i] + alpha1;
	}

	// step2: lines 5-6 - extract s bits (s = 8)
	IntFp *index = new IntFp[dim];
	gelu_extractBits(x1, index, h, s, dim);

    // TODO: 下面本来是不用的，但是index如果大于len，后面读的时候会段错误
	uint64_t *witness = new uint64_t[dim];
	for (int i = 0; i < dim; i++){
		if (party == ALICE)
			witness[i] = rand() % (uint64_t)len;
		index[i] = IntFp(witness[i], ALICE);
	}
	//

	cout << "extractBits end" << endl;

	// step3: lines 7-9 - LUT obtain coff + mult
	LUTTwoValueIntFp *LUTGeLU = new LUTTwoValueIntFp(party);
	vector<uint64_t> coff1;
    vector<uint64_t> coff2;
	for (int i = 0; i < len; i++){
		coff1.push_back(i);
		coff2.push_back(i);
	}
	LUTGeLU->LUTTwoValueinit(coff1, coff2);

	cout << "LUTGeLUinit end" << endl;

	// uint64_t *a = new uint64_t[dim];
	// uint64_t *b = new uint64_t[dim];
	// uint64_t *ori_y = new uint64_t[dim];
	uint64_t a;
	uint64_t b;
	uint64_t ori_y;
	// IntFp *am = new IntFp[dim];
	// IntFp *bm = new IntFp[dim];
	IntFp am, bm;
	IntFp *zm = new IntFp[dim];

	// IntFp *compare_in = new IntFp[dim];

	for (int i = 0; i < dim; i++)
	{
		// cout << "This is " << i << "-th" << " begin" << endl;
		if (party == ALICE)
		{
			// cout << "start ALICE" << endl;

			uint64_t k = (uint64_t)HIGH64(index[i].value);

			// cout << "k end" << endl;

			a = LUTGeLU->writes_a[k];
			b = LUTGeLU->writes_b[k];

            // cout << "a and b end" << endl;

			uint64_t t1 = (uint64_t)HIGH64(x1[i].value);
			uint64_t t2 = (uint64_t)HIGH64(x[i].value);

			// cout << "t1 and t2 end" << endl;

			ori_y = t1 > 0 ? (t1 > beta ? t2 : mod(mult_mod(a, t1) + b)) : 0;

			// cout << "ori_y end" << endl;
		}

		// !!!!!!! 真实输出应该是这样！！！！！！
		y[i] = IntFp(ori_y, ALICE);

		// cout << "true MACed output generation end" << endl;

		am = IntFp(a, ALICE);
		bm = IntFp(b, ALICE);

		zm[i] = am * x[i] + bm; // zm 应该这样计算

		LUTGeLU->LUTTwoValueread(index[i], am, bm);

		// cout << "LUTGeLUread end" << endl;

		// compare_in[i] = x1[i] + (0 - beta);

		// cout << "This is " << i << "-th" << " end" << endl;
	}

	cout << "start LUTGeLUcheck" << endl;

	// LUTGeLU->LUTTwoValuecheck();
	delete LUTGeLU;

	cout << "LUTGeLUcheck end" << endl;

	// step4: compare TODO: 此处应注意是否应该输入 0 和 beta，或许应该输入两者在Fp下的值？？？
	IntFp *compare_result1 = new IntFp[dim];
	IntFp *compare_result2 = new IntFp[dim];
	uint64_t zero = 0;
	ZKCompareConstant(x1, zero, compare_result1, dim);
	ZKCompareConstant(x1, beta, compare_result2, dim);

	cout << "ZKCompareConstant end" << endl;

	// step5: 计算GeLU()最终输出，并进一步将两个输出相减
	// IntFp *tmp_y = new IntFp[dim];
	for (int i = 0; i < dim; i++)
	{
		zm[i] = compare_result2[i] * x[i] + (compare_result1[i] + compare_result2[i].negate()) * zm[i];
		zm[i] = y[i] + zm[i].negate();
	}

	// step6: checkzero
	bool res = batch_reveal_check_zero(zm, dim);
	if (!res)
		error("batch_reveal_check_zero failed");

	cout << "finish check" << endl;

	
	delete[] x1;
	// delete[] a;
	// delete[] b;
	// delete[] ori_y;
	// delete[] am;
	// delete[] bm;
	delete[] zm;
	// delete[] compare_in;
	delete[] compare_result1;
	delete[] compare_result2;

	double time2 = time_from(start);

	std::cout << "time: " << time2 / 1000 << " ms\t " << party << endl;

	uint64_t com2 = comm(ios) - com1;
	std::cout << "communication (KB): " << com2 / 1024.0 << std::endl;
}

void ZKExp(IntFp *x, IntFp *y, int dim)  // 向量中每个元素求指数
{
	// step 1: DigitDec
	IntFp *xDigDec = new IntFp[dim * EXP_LUT_NUM];
	uint64_t *digit_size = new uint64_t[EXP_LUT_NUM];
	// memset(digit_size, EXP_DIGIT_LEN, EXP_LUT_NUM * sizeof(uint64_t));  // 这样写结果不对
	for (int i = 0; i < EXP_LUT_NUM - 1; i++){
		digit_size[i] = EXP_DIGIT_LEN;
	}
	digit_size[EXP_LUT_NUM - 1] = EXP_N - EXP_DIGIT_LEN * (EXP_LUT_NUM - 1);
	ZKPositiveDigDec(x, xDigDec, digit_size, EXP_LUT_NUM, dim);

	// step 2: LUTexp
	IntFp *yDigDec = new IntFp[dim * EXP_LUT_NUM];
	uint64_t digit_field = 0;
	uint64_t value_field = 0;
	for (int i = 0; i < dim; i++){
		for (int j = 0; j < EXP_LUT_NUM; j++){
			IntFp digit = xDigDec[i * EXP_LUT_NUM + j];
			if (party == ALICE){
				digit_field = (uint64_t)HIGH64(digit.value);
				value_field = LUTexp[j]->writes_value[digit_field];
			}
			yDigDec[i * EXP_LUT_NUM + j] = IntFp(value_field, ALICE);
			LUTexp[j]->LUTread(digit, yDigDec[i * EXP_LUT_NUM + j]);
		}

		y[i] = yDigDec[i * EXP_LUT_NUM];
	}

	// step 3: multiplication + truncation
	for (int i = 1; i < EXP_LUT_NUM; i++){
		for (int j = 0; j < dim; j++){
			y[j] = y[j] * yDigDec[j * EXP_LUT_NUM + i];
		}
		ZKpositiveTruncAny(y, y, dim, SCALE);
	}

	delete[] xDigDec;
	delete[] digit_size;
	delete[] yDigDec;
}

void ZKExtend(IntFp *x, IntFp *k, IntFp *y, int dim)    // 用于Div
{
	for (int i = 0; i < dim; i++){
		// step 1: compute mz
		uint64_t z = 0;
		if (party == ALICE){
			uint64_t curr_k = (uint64_t)HIGH64(k[i].value);
			z = 1ULL << curr_k;
		}

		IntFp mz = IntFp(z, ALICE);
		LUTextend->LUTread(k[i], mz);

		// step 2: compute y
		y[i] = x[i] * mz;
	}
}

void ZKDiv(IntFp *x, IntFp *y, int dim)  //一个元素的倒数
{
	// step 1: msnzb
	IntFp *k = new IntFp[dim];
	ZKmsnzb(x, k, dim);
	// cout << "finish step 1" << endl;

	// step 2: extend
	IntFp *extendLen = new IntFp[dim]; 
	for (int i = 0; i < dim; i++){
		extendLen[i] = k[i].negate() + (DIV_N - 1);
	}
	IntFp *z = new IntFp[dim];
	ZKExtend(x, extendLen, z, dim);

	// step 3: DigitDec
	for (int i = 0; i < dim; i++){
		z[i] = z[i] + (PR - (1ULL << (DIV_N - 1)));     
	}
	IntFp *z1 = new IntFp[dim];
	IntFp *z0 = new IntFp[dim];
	ZKPositiveDigDecAny(z, z1, z0, DIV_M, DIV_N - 1 - DIV_M, dim);

	// step 4: LUT + y'trunc
	IntFp *yprime = new IntFp[dim];
	IntFp *yprimeTrunc = new IntFp[dim];
	int32_t tmp = 1ULL << DIV_M;
	uint64_t coff1 = 0;
	uint64_t coff2 = 0;
	for (int i = 0; i < dim; i++){
		if (party == ALICE){
			// a \in (1/2, 1)
			uint64_t digit = (uint64_t)HIGH64(z1[i].value);
			coff1 = LUTdiv->writes_a[digit];
        	// b \in (1/4, 1)
			coff2 = LUTdiv->writes_b[digit];
		}
		IntFp a = IntFp(coff1, ALICE);
		IntFp b = IntFp(coff2, ALICE);
		LUTdiv->LUTTwoValueread(z1[i], a, b);

		yprime[i] = (b * z0[i]).negate() + a;
	}

	ZKpositiveTruncAny(yprime, yprimeTrunc, dim, DIV_N - 1);

	// step 5: extend + trunc
	// for (int i = 0; i < dim; i++){
	// 	extendLen[i] = k[i].negate() + DIV_N;
	// }
	ZKExtend(yprimeTrunc, extendLen, yprime, dim);  // 重用了yprime,放置ZKExtend的输出
	ZKpositiveTruncAny(yprime, y, dim, DIV_N - SCALE - 1);

	delete[] k;
	delete[] extendLen;
	delete[] z;
	delete[] z1;
	delete[] z0;
	delete[] yprime;
	delete[] yprimeTrunc;
}

void ZKExtendSqrt(IntFp *x, IntFp *k, IntFp *y, int dim)    // 用于rSqrt
{
	for (int i = 0; i < dim; i++){
		// step 1: compute mz
		uint64_t z = 0;
		if (party == ALICE){
			uint64_t curr_k = (uint64_t)HIGH64(k[i].value);
			z = LUTsqrtExtend->writes_value[curr_k];
		}

		IntFp mz = IntFp(z, ALICE);
		LUTsqrtExtend->LUTread(k[i], mz);

		// step 2: compute y
		y[i] = x[i] * mz;
	}
}

void ZKrSqrt(IntFp *x, IntFp *y, int dim, int iter)
{
	// step 1: msnzb
	IntFp *k = new IntFp[dim];
	ZKmsnzb(x, k, dim);

	// step 2: extend
	IntFp *extendLen = new IntFp[dim]; 
	for (int i = 0; i < dim; i++){
		extendLen[i] = k[i].negate() + (SQRT_N - 1);
	}
	IntFp *z = new IntFp[dim];
	ZKExtend(x, extendLen, z, dim);

	// step 3: DigitDec
	IntFp *k1 = new IntFp[dim];
	IntFp *k0 = new IntFp[dim];
	ZKPositiveDigDecAny(k, k1, k0, ceil(log2(SQRT_N)) - 1, 1, dim);
	IntFp *zprime = new IntFp[dim];
	for (int i = 0; i < dim; i++){
		zprime[i] = z[i] + (PR - (1ULL << (SQRT_N - 1)));     
	}
	IntFp *z1 = new IntFp[dim];
	IntFp *z0 = new IntFp[dim];
	ZKPositiveDigDecAny(zprime, z1, z0, SQRT_M, SQRT_N - 1 - SQRT_M, dim);

	// step 4: LUTsqrt
	IntFp *abeforeTrunc = new IntFp[dim];
	IntFp *a = new IntFp[dim];
	IntFp *b = new IntFp[dim];
	IntFp *c = new IntFp[dim];
	uint64_t value = 0;
	for (int i = 0; i < dim; i++){
		IntFp sqrt_index = z1[i] * 2 + k0[i];
		if (party == ALICE){
			uint64_t index = (uint64_t)HIGH64(sqrt_index.value);
			value = LUTsqrt->writes_value[index];
		}
		IntFp mvalue = IntFp(value, ALICE);
		LUTsqrt->LUTread(sqrt_index, mvalue);

	// step 5: compute a, b, c
		abeforeTrunc[i] = (k0[i] + 1) * z[i];
		b[i] = mvalue;
		c[i] = mvalue;
	}
	ZKpositiveTruncAny(abeforeTrunc, a, dim, SQRT_N - 1 - SCALE);

	// step 6: iteration
	IntFp *cbeforeTrunc = new IntFp[dim];
	for (int i = 0; i < iter; i++){
		for (int j = 0; j < dim; j++){
			abeforeTrunc[j] = b[j] * b[j] * a[j];
		}
		ZKpositiveTruncAny(abeforeTrunc, a, dim, 2 * SCALE);
		for (int j = 0; j < dim; j++){
			b[j] = a[j].negate() + (3 * (1ULL << SCALE));  
			cbeforeTrunc[j] = c[j] * b[j];
		}
		ZKpositiveTruncAny(cbeforeTrunc, c, dim, SCALE + 1);
	}

	// step 7: extend and truncation
	IntFp *CI = new IntFp[dim];
	ZKExtendSqrt(c, k, CI, dim);
	uint64_t trunclen = (SQRT_N - SCALE)/2;
	ZKpositiveTruncAny(CI, y, dim, trunclen);

	delete[] k;
	delete[] extendLen;
	delete[] z;
	delete[] k1;
	delete[] k0;
	delete[] zprime;
	delete[] z1;
	delete[] z0;
	delete[] abeforeTrunc;
	delete[] a;
	delete[] b;
	delete[] c;
	delete[] cbeforeTrunc;
	delete[] CI;
}

void ZKSoftmax(IntFp *x, IntFp *y, int rows, int cols)  // x: rows*cols  y: rows
{
	IntFp *max = new IntFp[rows];
	IntFp *z = new IntFp[rows * cols];
	IntFp *ez = new IntFp[rows * cols];
	IntFp *sum = new IntFp[rows];
	IntFp *t = new IntFp[rows];
	IntFp *ybeforeTrunc = new IntFp[rows * cols];
	
	// step 1: Max: 每一行有一个max
	ZKMax(x, max, rows, cols);

	// step 2: exp
	for (int i = 0; i < rows; i++){
		for (int j = 0; j < cols; j++){
			z[i * cols + j] = x[i * cols + j].negate() + max[i];
		}
	}
	ZKExp(z, ez, rows * cols);

	// step 3: sum
	for (int i = 0; i < rows; i++){
		sum[i] = ez[i * cols];
		for (int j = 1; j < cols; j++){
			sum[i] = sum[i] + ez[i * cols + j];
		}
	}

	// step 4: div
	ZKDiv(sum, t, rows);
	
	// step 4: mult + trunc
	for (int i = 0; i < rows; i++){
		for (int j = 0; j < cols; j ++){
			ybeforeTrunc[i * cols + j] = t[i] * ez[i * cols + j];
		}
	}
	ZKpositiveTruncAny(ybeforeTrunc, y, rows * cols, SCALE);

	delete[] max;
	delete[] z;
	delete[] ez;
	delete[] sum;
	delete[] t;
	delete[] ybeforeTrunc;
}

void ZKLayerNorm(IntFp *x, IntFp *y, IntFp *gamma, IntFp *beta, int rows, int cols)  // gamma: rows; beta: rows
{
	uint64_t cols_field = Real2Field(1.0/cols, SCALE);

	// step 1: compute mu + trunc
	IntFp *mu = new IntFp[rows];
	IntFp *sum = new IntFp[rows];
	for (int i = 0; i < rows; i++){
		sum[i] = x[i * cols];
		for (int j = 1; j < cols; j++){
			sum[i] = sum[i] + x[i * cols + j];
		}
		mu[i] = sum[i] * cols_field;
	}
	ZKgeneralTruncAny(mu, mu, rows, SCALE);
	cout << "end step 1" << endl;

	// step 2: compute sigma
	IntFp *sigma = new IntFp[rows];
	for (int i = 0; i < rows; i++){
		IntFp tmp = x[i * cols] + mu[i].negate();
		sum[i] = tmp * tmp;
		for (int j = 1; j < cols; j++){
			tmp = x[i + cols + j] + mu[i].negate();
			sum[i] = sum[i] + (tmp * tmp);
		}
		sigma[i] = sum[i] * cols_field;
	}
	ZKpositiveTruncAny(sigma, sigma, rows, 2 * SCALE);
	cout << "end step 2" << endl;

	// step 3: compute t - sqrt
	IntFp *t = new IntFp[rows];
	ZKrSqrt(sigma, t, rows, 1);
	cout << "end step 3" << endl;

	// step 4: compute z
	IntFp *z = new IntFp[rows * cols];
	for (int i = 0; i < rows; i++){
		for (int j = 0; j < cols; j++){
			z[i * cols + j] = t[i] * (x[i * cols + j] + mu[i].negate());
		}
	}
	ZKgeneralTruncAny(z, z, rows * cols, SCALE);
	cout << "end step 4" << endl;

	// step 5: compute y
	IntFp *ybeforeTrunc = new IntFp[rows * cols];
	for (int i = 0; i < rows; i++){
		for (int j = 0; j < cols; j++){
			ybeforeTrunc[i * cols + j] = z[i * cols + j] * gamma[i] + beta[i];
		}
	}
	cout << "end circle" << endl;
	ZKgeneralTruncAny(ybeforeTrunc, y, rows * cols, SCALE);
	cout << "end step 5" << endl;

	delete[] mu;
	delete[] sum;
	delete[] sigma;
	delete[] t;
	delete[] z;
	delete[] ybeforeTrunc;
}



int main(int argc, char **argv)
{
	parse_party_and_port(argv, &party, &port);
	BoolIO<NetIO> *ios[threads];
	for (int i = 0; i < threads; ++i)
		ios[i] = new BoolIO<NetIO>(new NetIO(party == ALICE ? nullptr : "127.0.0.1", port + i), party == ALICE);

	std::cout << std::endl
			  << "------------ Zero-knowledge proof test ------------" << std::endl
			  << std::endl;

	setup_zk_bool<BoolIO<NetIO>>(ios, threads, party);
	setup_zk_arith<BoolIO<NetIO>>(ios, threads, party, true);

	sync_zk_bool<BoolIO<NetIO>>();

	uint64_t *witness = new uint64_t[dim];
	memset(witness, 0, dim * sizeof(uint64_t));


    // cout << "GeLU test" << endl;
	// IntFp *x = new IntFp[dim];
	// IntFp *y = new IntFp[dim];
	// for (int i = 0; i < dim; i++)
	// {
	// 	if (party == ALICE)
	// 		witness[i] = rand() % PR;
	// 	x[i] = IntFp(witness[i], ALICE);
	// }
	// ZKGeLU(ios, party, dim, x, y);

	startComputation(party);

	// cout << "ZKPositiveDigDec test" << endl;
	// int num_digits = 6;
	// uint64_t *digit_size = new uint64_t[num_digits];
	// digit_size[0] = 7;
	// digit_size[1] = 8;
	// digit_size[2] = 12;
	// digit_size[3] = 12;
	// digit_size[4] = 12;
	// digit_size[5] = 10;
	// IntFp *xDigDec = new IntFp[dim];
	// IntFp *yDigDec = new IntFp[num_digits * dim];
	// for (int i = 0; i < dim; i++){
	// 	if (party == ALICE){
	// 		witness[i] = rand();
	// 		witness[i] = witness[i] & (1ULL << ((uint64_t)ceil(log2(PR)) - 1) - 1);
	// 		witness[i] = witness[i] % PR;
	// 	}
	// 	xDigDec[i] = IntFp(witness[i], ALICE);
	// }
	// ZKPositiveDigDec(xDigDec, yDigDec, digit_size, num_digits, dim);
	// /****************************/
	// /**** verify correctness ****/
	// /****************************/
	// if (party == ALICE){
	// 	for (int i = 0; i < dim; i++){
	// 		uint64_t shiftlen = 0;
	// 		uint64_t digit_mask = (1ULL << digit_size[0]) - 1;
	// 		uint64_t digit = (witness[i] >> shiftlen) & digit_mask;
	// 		uint64_t nm_ydigit = (uint64_t)HIGH64(yDigDec[i * num_digits].value);
	// 		if (digit != nm_ydigit){
	// 			cout << "fault !!!" << endl;
	// 		}

	// 		for (int j = 1; j < num_digits; j++){
	// 			shiftlen += digit_size[j - 1];
	// 			digit_mask = (1ULL << digit_size[j]) - 1;
	// 			digit = (witness[i] >> shiftlen) & digit_mask;
	// 			nm_ydigit = (uint64_t)HIGH64(yDigDec[i * num_digits + j].value);
	// 			if (digit != nm_ydigit){
	// 				cout << "fault !!!" << endl;
	// 			}
	// 		}
	// 	}
	// }
    
	// cout << "ZKpositiveTrunc12 test" << endl;
    // IntFp *x = new IntFp[dim];
	// IntFp *y = new IntFp[dim];
	// for (int i = 0; i < dim; i++)
	// {
	// 	if (party == ALICE){
	// 		witness[i] = rand();
	// 		if (witness[i] < 0){
	// 			witness[i] = -1 * witness[i];
	// 		}
	// 		witness[i] = witness[i] % PR;
	// 	} 
	// 	x[i] = IntFp(witness[i], ALICE);
	// }
	// ZKpositiveTrunc12(x, y, dim, 12);

	// cout << "ZKmsnzb test" << endl;
    // IntFp *x = new IntFp[dim];
	// IntFp *y = new IntFp[dim];
	// for (int i = 0; i < dim; i++){
	// 	if (party == ALICE){
	// 		while (witness[i] == 0){
	// 			witness[i] = rand();
	// 		}
	// 		witness[i] = rand() % ((BIT_LENGTH - 1) / 2 + 1);
	// 		witness[i] = rand() % PR;
	// 	} 
	// 	x[i] = IntFp(witness[i], ALICE);
	// }
	// ZKmsnzb(x, y, dim);
	// /****************************/
	// /**** verify correctness ****/
	// /****************************/
	// if (party == ALICE){
	// 	for (int i = 0; i < dim; i++){
	// 		uint64_t msnzb = floor(log2(witness[i]));
	// 		uint64_t nm_msnzb = (uint64_t)HIGH64(y[i].value);
	// 		if (msnzb != nm_msnzb){
	// 			cout << "fault !!!" << endl;
	// 		}
	// 	}
	// }

	// cout << "ZKSoftmax test" << endl;
    // IntFp *x = new IntFp[rows * cols];
	// IntFp *y = new IntFp[rows * cols];
	// for (int i = 0; i < rows * cols; i++){
	// 	if (party == ALICE){
	// 		witness[i] = rand() % PR;
	// 	} 
	// 	x[i] = IntFp(witness[i], ALICE);
	// }
	// ZKSoftmax(x, y, rows, cols);

	// cout << "ZKLayerNorm test" << endl;
    // IntFp *x = new IntFp[rows * cols];
	// IntFp *y = new IntFp[rows * cols];
	// uint64_t *gammaa = new uint64_t[rows];
	// uint64_t *betaa = new uint64_t[rows];
	// IntFp *gamma = new IntFp[rows];
	// IntFp *beta = new IntFp[rows];
	// for (int i = 0; i < rows * cols; i++){
	// 	if (party == ALICE){
	// 		witness[i] = rand() % PR;
	// 	} 
	// 	x[i] = IntFp(witness[i], ALICE);
	// }
	// for (int i = 0; i < rows; i++){
	// 	if (party == ALICE){
	// 		gammaa[i] = rand() % PR;
	// 		betaa[i] = rand() % PR;
	// 	} 
	// 	gamma[i] = IntFp(gammaa[i], ALICE);
	// 	beta[i] = IntFp(betaa[i], ALICE);
	// }
	// ZKLayerNorm(x, y, gamma, beta, rows, cols);

	// cout << "ZKgeneralDigDec test" << endl;
	// int num_digits = 6;
	// uint64_t *digit_size = new uint64_t[num_digits];
	// digit_size[0] = 7;
	// digit_size[1] = 8;
	// digit_size[2] = 12;
	// digit_size[3] = 12;
	// digit_size[4] = 12;
	// digit_size[5] = 10;
	// IntFp *xDigDec = new IntFp[dim];
	// IntFp *yDigDec = new IntFp[num_digits * dim];
	// for (int i = 0; i < dim; i++)
	// {
	// 	if (party == ALICE)
	// 		witness[i] = rand() % PR;
	// 	xDigDec[i] = IntFp(witness[i], ALICE);
	// }
	// ZKgeneralDigDec(xDigDec, yDigDec, digit_size, num_digits, dim);

	// cout << "ZKgeneralDigDecAny test" << endl;
	// int num_digits = 3;
	// uint64_t *digit_size = new uint64_t[num_digits];
	// digit_size[0] = 7;
	// digit_size[1] = 8;
	// digit_size[2] = 46;
	// assert(digit_size[0] + digit_size[1] + digit_size[2] == 61);
	// IntFp *xDigDec = new IntFp[dim];
	// IntFp *yDigDec = new IntFp[num_digits * dim];
	// for (int i = 0; i < dim; i++)
	// {
	// 	if (party == ALICE){
	// 		witness[i] = rand() % PR;
	// 	}
	// 	xDigDec[i] = IntFp(witness[i], ALICE);
	// }
	// ZKgeneralDigDecAny(xDigDec, yDigDec, digit_size, num_digits, dim);
	// /****************************/
	// /**** verify correctness ****/
	// /****************************/
	// if (party == ALICE){
	// 	for (int i = 0; i < dim; i++){
	// 		uint64_t shiftlen = 0;
	// 		for (int j = 0; j < num_digits; j++){
	// 			uint64_t curr_digit_size = digit_size[j];
	// 			uint64_t curr_mask = (1ULL << curr_digit_size) - 1;
	// 			uint64_t ori_y = (witness[i] >> shiftlen) & curr_mask;

	// 			uint64_t nm_y = (uint64_t)HIGH64(yDigDec[i * num_digits + j].value);
	// 			if (ori_y != nm_y){
	// 				cout << "fault !!!" << endl;
	// 			}
	// 			shiftlen += digit_size[j];
	// 		}
	// 	}
	// }

	// cout << "test F2R and R2F" << endl;
	// if (party == ALICE){
	// 	double positive_x = 51.23;
	// 	uint64_t positive_xfield = Real2Field(positive_x, SCALE);
	// 	cout << "positive_xfield = " << positive_xfield << endl;
	// 	double negative_x = -51.23;
	// 	uint64_t negative_xfield = Real2Field(negative_x, SCALE);
	// 	cout << "negative_xfield = " << negative_xfield << endl;

	// 	uint64_t p_x = 536;
	// 	double x_real = Field2Real(p_x, SCALE);
	// 	cout << "positive_xreal = " << x_real << endl;
	// 	uint64_t n_x = PR - 536;
	// 	x_real = Field2Real(n_x, SCALE);
	// 	cout << "negative_xreal = " << x_real << endl;
	// }
	
	// cout << "ZKgeneralTrunc12 test" << endl;
    // IntFp *x = new IntFp[dim];
	// IntFp *y = new IntFp[dim];
	// for (int i = 0; i < dim; i++)
	// {
	// 	if (party == ALICE){
	// 		witness[i] = rand() % PR;
	// 	} 
	// 	x[i] = IntFp(witness[i], ALICE);
	// }
	// ZKgeneralTrunc12(x, y, dim, 12);

	// cout << "ZKgeneralTruncAny test" << endl;
    // IntFp *x = new IntFp[dim];
	// IntFp *y = new IntFp[dim];
	// uint64_t trunclen = SCALE;
	// for (int i = 0; i < dim; i++){
	// 	if (party == ALICE){
	// 		witness[i] = rand() % PR;
	// 	} 
	// 	x[i] = IntFp(witness[i], ALICE);
	// }
	// ZKgeneralTruncAny(x, y, dim, trunclen);
	// /****************************/
	// /**** verify correctness ****/
	// /****************************/
	// if (party == ALICE){
	// 	int64_t witness_int = 0;
	// 	uint64_t output_real = 0;
	// 	uint64_t output_prot = 0;
	// 	for (int i = 0; i < dim; i++){
	// 		witness_int = witness[i] > (PR-1)/2 ? witness[i] - PR : witness[i];
	// 		witness_int = witness_int >> trunclen;
	// 		output_real = witness_int < 0 ? PR + witness_int : witness_int;
	// 		output_prot = (uint64_t)HIGH64(y[i].value);
	// 		if (output_real != output_prot){
	// 			cout << "fault !!!" << endl;
	// 		}
	// 	}
	// }

	// cout << "ZKCompareShift test" << endl;
	// uint64_t shiftlen = ceil(log2(PR)) - 1;
    // IntFp *x = new IntFp[dim];
	// IntFp *y = new IntFp[dim];
	// for (int i = 0; i < dim; i++)
	// {
	// 	if (party == ALICE){
	// 		witness[i] = rand() % PR;
	// 	} 
	// 	x[i] = IntFp(witness[i], ALICE);
	// }
	// ZKCompareShift(x, y, shiftlen, dim);
	// /****************************/
	// /**** verify correctness ****/
	// /****************************/
	// if (party == ALICE){
	// 	for (int i = 0; i < dim; i++){
	// 		uint64_t ori_y = 0;
	// 		if ( (witness[i] > (1ULL << shiftlen)) || (witness[i] == (1ULL << shiftlen)) ){
	// 			ori_y = 1;
	// 		}
	// 		uint64_t nm_y = (uint64_t)HIGH64(y[i].value);
	// 		if (ori_y != nm_y){
	// 			cout << "fault !!!" << endl;
	// 		}
	// 	}
	// }

	// cout << "ZKCompareConstant test" << endl;
	// uint64_t y = (PR - 1)/2;
    // IntFp *x = new IntFp[dim];
	// IntFp *z = new IntFp[dim];
	// for (int i = 0; i < dim; i++)
	// {
	// 	if (party == ALICE){
	// 		witness[i] = rand() % PR;
	// 	} 
	// 	x[i] = IntFp(witness[i], ALICE);
	// }
	// ZKCompareConstant(x, y, z, dim);
	// /****************************/
	// /**** verify correctness ****/
	// /****************************/
	// if (party == ALICE){
	// 	for (int i = 0; i < dim; i++){
	// 		uint64_t ori_z = 0;
	// 		if ( (witness[i] < y) || (witness[i] == y) ){
	// 			ori_z = 1;
	// 		}
	// 		uint64_t nm_z = (uint64_t)HIGH64(z[i].value);
	// 		if (ori_z != nm_z){
	// 			cout << "fault !!!" << endl;
	// 		}
	// 	}
	// }

	// cout << "ZKFpCompare test" << endl;
	// uint64_t *witness1 = new uint64_t[dim];
    // IntFp *x = new IntFp[dim];
	// IntFp *y = new IntFp[dim];
	// IntFp *z = new IntFp[dim];
	// for (int i = 0; i < dim; i++)
	// {
	// 	if (party == ALICE){
	// 		witness[i] = rand() % PR;
	// 		witness1[i] = rand() % PR;
	// 	} 
	// 	x[i] = IntFp(witness[i], ALICE);
	// 	y[i] = IntFp(witness1[i], ALICE);
	// }
	// ZKFpCompare(x, y, z, dim);
	// /****************************/
	// /**** verify correctness ****/
	// /****************************/
	// if (party == ALICE){
	// 	for (int i = 0; i < dim; i++){
	// 		uint64_t ori_z = 0;
	// 		if ( witness[i] < witness1[i] ){
	// 			ori_z = 1;
	// 		}
	// 		uint64_t nm_z = (uint64_t)HIGH64(z[i].value);
	// 		if (ori_z != nm_z){
	// 			cout << "fault !!!" << endl;
	// 		}
	// 	}
	// }

	// cout << "ZKFpCompare test" << endl;
	// uint64_t *witness1 = new uint64_t[dim];
    // IntFp *x = new IntFp[dim];
	// IntFp *y = new IntFp[dim];
	// IntFp *z = new IntFp[dim];
	// for (int i = 0; i < dim; i++)
	// {
	// 	if (party == ALICE){
	// 		witness[i] = rand() % PR;
	// 		witness1[i] = rand() % PR;
	// 		while (witness1[i] == (PR - 1))   //保证+1后不溢出
	// 		{
	// 			witness[i] = rand();
	// 		}
	// 	} 
	// 	x[i] = IntFp(witness[i], ALICE);
	// 	y[i] = IntFp(witness1[i], ALICE);
	// }
	// ZKFpCompareLEQ(x, y, z, dim);
	// /****************************/
	// /**** verify correctness ****/
	// /****************************/
	// if (party == ALICE){
	// 	for (int i = 0; i < dim; i++){
	// 		uint64_t ori_z = 0;
	// 		if ( witness[i] <= witness1[i] ){
	// 			ori_z = 1;
	// 		}
	// 		uint64_t nm_z = (uint64_t)HIGH64(z[i].value);
	// 		if (ori_z != nm_z){
	// 			cout << "fault !!!" << endl;
	// 		}
	// 	}
	// }

	// cout << "ZKMax test" << endl;
    // IntFp *x = new IntFp[rows * cols];
	// IntFp *y = new IntFp[rows];
	// for (int i = 0; i < rows * cols; i++){
	// 	if (party == ALICE){
	// 		witness[i] = rand() % PR;
	// 	} 
	// 	x[i] = IntFp(witness[i], ALICE);
	// }
	// ZKMax(x, y, rows, cols); 
	// /****************************/
	// /**** verify correctness ****/
	// /****************************/
	// if (party == ALICE){
	// 	for (int i = 0; i < rows; i++){
	// 		uint64_t max_y = witness[i * cols];
	// 		for (int j = 1; j < cols; j++){
	// 			if ( witness[i * cols + j] >=  max_y){
	// 				max_y = witness[i * cols + j];
	// 			}
	// 		}
	// 		uint64_t nm_y = (uint64_t)HIGH64(y[i].value);
	// 		if (max_y != nm_y){
	// 			cout << "fault !!!" << endl;
	// 		}
	// 	}
	// }

	// cout << "ZKPositiveDigDecAny test" << endl;
	// IntFp *x = new IntFp[dim];
	// IntFp *yhigh = new IntFp[dim];
	// IntFp *ylow = new IntFp[dim];
	// for (int i = 0; i < dim; i++){
	// 	if (party == ALICE){
	// 		witness[i] = rand();
	// 		witness[i] = witness[i] & (1ULL << ((uint64_t)ceil(log2(PR)) - 1) - 1);
	// 		witness[i] = witness[i] % PR;
	// 	}
	// 	x[i] = IntFp(witness[i], ALICE);
	// }
	// uint64_t lowsize = 23;
	// uint64_t highsize = ceil(log2(PR)) - lowsize - 1;
	// ZKPositiveDigDecAny(x, yhigh, ylow, highsize, lowsize, dim);
	// /****************************/
	// /**** verify correctness ****/
	// /****************************/
	// if (party == ALICE){
	// 	for (int i = 0; i < dim; i++){
	// 		uint64_t shiftlen = 0;
	// 		uint64_t digit_mask = (1ULL << lowsize) - 1;
	// 		uint64_t low = (witness[i] >> shiftlen) & digit_mask;
	// 		uint64_t nm_ylow = (uint64_t)HIGH64(ylow[i].value);
	// 		if (low != nm_ylow){
	// 			cout << "fault !!!" << endl;
	// 		}

	// 		shiftlen = lowsize;
	// 		digit_mask = (1ULL << highsize) - 1;
	// 		uint64_t high = (witness[i] >> shiftlen) & digit_mask;
	// 		uint64_t nm_yhigh = (uint64_t)HIGH64(yhigh[i].value);
	// 		if (high != nm_yhigh){
	// 			cout << "fault !!!" << endl;
	// 		}
	// 	}
	// }

	// cout << "ZKpositiveTruncAny test" << endl;
	// IntFp *x = new IntFp[dim];
	// IntFp *y = new IntFp[dim];
	// for (int i = 0; i < dim; i++){
	// 	if (party == ALICE){
	// 		witness[i] = rand() %  ((BIT_LENGTH - 1) / 2 + 1);
	// 		witness[i] = witness[i] % PR;
	// 	}
	// 	x[i] = IntFp(witness[i], ALICE);
	// }
	// uint64_t trunclen = 6;
	// ZKpositiveTruncAny(x, y, dim, trunclen);
	// /****************************/
	// /**** verify correctness ****/
	// /****************************/
	// if (party == ALICE){
	// 	for (int i = 0; i < dim; i++){
	// 		uint64_t digit_mask = (1ULL << (BIT_LENGTH - trunclen)) - 1;
	// 		uint64_t ori_y = (witness[i] >> trunclen) & digit_mask;
	// 		uint64_t nm_y = (uint64_t)HIGH64(y[i].value);
	// 		if (ori_y != nm_y){
	// 			cout << "fault !!!" << endl;
	// 		}
	// 	}
	// }

	// cout << "ZKExp test" << endl;
	// IntFp *x = new IntFp[dim];
	// IntFp *y = new IntFp[dim];
	// for (int i = 0; i < dim; i++){
	// 	if (party == ALICE){
	// 		witness[i] = (uint32_t)rand();
	// 		witness[i] = witness[i] & ((1ULL << EXP_N) - 1);  
	// 		witness[i] = witness[i] % PR;
	// 	}
	// 	x[i] = IntFp(witness[i], ALICE);
	// }
	// ZKExp(x, y, dim);
	// /****************************/
	// /**** verify correctness ****/
	// /****************************/
	// uint64_t total_err_fixed = 0;
    // uint64_t max_ULP_err_fixed = 0;
	// if (party == ALICE){
	// 	uint64_t last_digit_bitlen = EXP_N - (EXP_DIGIT_LEN) * (EXP_LUT_NUM - 1);
	// 	for (int i = 0; i < dim; i++){
	// 		// 计算每个分块的结果
	// 		uint64_t *value_field = new uint64_t[EXP_LUT_NUM];
	// 		for (int j = 0; j < EXP_LUT_NUM - 1; j++){
	// 			uint64_t digit = (witness[i] >> (j * EXP_DIGIT_LEN)) & ((1ULL << EXP_DIGIT_LEN) - 1);
	// 			double digit_real = double(digit) / double(1ULL << SCALE);
    //         	double value_real = exp(-1 * (digit_real * (1ULL << EXP_DIGIT_LEN * j)));
	// 			value_field[j] = value_real * (1ULL << SCALE);
	// 		}
	// 		uint64_t digit = (witness[i] >> ((EXP_LUT_NUM - 1) * EXP_DIGIT_LEN)) & ((1ULL << last_digit_bitlen) - 1);
	// 		double digit_real = double(digit) / double(1ULL << SCALE);
    //         double value_real = exp(-1 * (digit_real * (1ULL << EXP_DIGIT_LEN * (EXP_LUT_NUM - 1))));
	// 		value_field[EXP_LUT_NUM - 1] = value_real * (1ULL << SCALE);
	// 		// 相乘
	// 		uint64_t ori_y = value_field[0];
	// 		for (int j = 1; j < EXP_LUT_NUM; j++){
	// 			ori_y = ori_y * value_field[j] >> SCALE;
	// 		}
	// 		uint64_t nm_y = (uint64_t)HIGH64(y[i].value);
	// 		if (ori_y != nm_y){
	// 			cout << "exp fault !!!" << "real: " << ori_y << ", protocol: " << nm_y << endl;
	// 		}
	// 		/****************************/
	// 		/****** verify ULPerror *****/
	// 		/****************************/
	// 		double witness_real = Field2Real(witness[i], SCALE);
	// 		double exp = std::exp(-1 * witness_real);
	// 		uint64_t exp_field = Real2Field(exp, SCALE);
	// 		uint64_t err_fixed = computeULPErr(ori_y, exp_field);
    //   		if (true)
    //   		{
    //     		cout << "ULP Error Fixed: " << ori_y << "," << exp_field << ","
    //          		<< err_fixed << endl;
    //   		}
    //   		total_err_fixed += err_fixed;
    //   		max_ULP_err_fixed = std::max(max_ULP_err_fixed, err_fixed);
	// 	}
	// 	cout << "Average ULP error fixed: " << total_err_fixed / dim << endl;
    // 	cout << "Total ULP error fixed: " << total_err_fixed << endl;
    // 	cout << "Max ULP error fixed: " << max_ULP_err_fixed << endl;
    // 	cout << "Number of tests fixed: " << dim << endl;
	// }

	// cout << "ZKDiv test" << endl;
	// IntFp *x = new IntFp[dim];
	// IntFp *y = new IntFp[dim];
	// for (int i = 0; i < dim; i++){
	// 	if (party == ALICE){
	// 		while (witness[i] == 0)
	// 		{
	// 			witness[i] = rand();
	// 		}
	// 		witness[i] = witness[i] & ((1ULL << DIV_N) - 1);  
	// 		witness[i] = witness[i] % PR;
	// 	}
	// 	x[i] = IntFp(witness[i], ALICE);
	// }
	// ZKDiv(x, y, dim);
	// /****************************/
	// /**** verify correctness ****/
	// /****************************/
	// uint64_t total_err_fixed = 0;
    // uint64_t max_ULP_err_fixed = 0;
	// if (party == ALICE){
	// 	for (int i = 0; i < dim; i++){
	// 		uint64_t msnzb = floor(log2(int64_t(witness[i])));
	// 		uint64_t z = witness[i] * (1ULL << (DIV_N - 1- msnzb));
	// 		uint64_t z0 = z & ((1ULL << (DIV_N - 1 - DIV_M)) - 1);   // 右移的部分必须加括号再-1啊，不然结果错误
	// 		uint64_t z1 = (z >> (DIV_N - 1 - DIV_M)) & ((1ULL << DIV_M) - 1);
	// 		uint64_t k = 1ULL << DIV_M;
	// 		// a \in (1/2, 1)
    //     	double p = 1 + (double(z1) / double(k));
    //     	double zz = (p * (p + (1.0 / double(k))));
    //     	double A0 = ((1.0 / double(k * 2)) + sqrt(zz)) / zz;
    //     	uint64_t scale = SCALE + DIV_N - 1;
    //     	uint64_t a = (A0 * (1ULL << scale));
    //     	// b \in (1/4, 1)
    //     	double A1 = 1.0 / zz;
    //     	uint64_t b = A1 * (1ULL << SCALE);
	// 		uint64_t ori_y = a - b * z0;
	// 		ori_y = (ori_y >> (DIV_N - 1)) * (1ULL << (DIV_N - 1 - msnzb));
	// 		ori_y = ori_y >> (DIV_N - 1 - SCALE);
	// 		uint64_t nm_y = (uint64_t)HIGH64(y[i].value);
	// 		if (ori_y != nm_y){
	// 			cout << "div fault !!!" << "real: " << ori_y << ", protocol: " << nm_y << endl;
	// 		}
	// 		/****************************/
	// 		/****** verify ULPerror *****/
	// 		/****************************/
	// 		double witness_real = Field2Real(witness[i], SCALE);
	// 		double div = 1.0/witness_real;
	// 		uint64_t div_field = Real2Field(div, SCALE);
	// 		uint64_t err_fixed = computeULPErr(ori_y, div_field);
    //   		if (err_fixed > 1)
    //   		{
    //     		cout << "ZKDiv ULP Error Fixed: " << ori_y << "," << div_field << ","
    //          		<< err_fixed << endl;
    //   		}
    //   		total_err_fixed += err_fixed;
    //   		max_ULP_err_fixed = std::max(max_ULP_err_fixed, err_fixed);
	// 	}
	// 	cout << "Average ULP error fixed: " << total_err_fixed / dim << endl;
    // 	cout << "Total ULP error fixed: " << total_err_fixed << endl;
    // 	cout << "Max ULP error fixed: " << max_ULP_err_fixed << endl;
    // 	cout << "Number of tests fixed: " << dim << endl;
	// }

	// cout << "ZKrSqrt test" << endl;
	// IntFp *x = new IntFp[dim];
	// IntFp *y = new IntFp[dim];
	// int iter = 1;
	// for (int i = 0; i < dim; i++){
	// 	if (party == ALICE){
	// 		while (witness[i] == 0)
	// 		{
	// 			witness[i] = rand();
	// 		}
	// 		witness[i] = witness[i] & ((1ULL << SQRT_N) - 1);    // TODO: 注意witness不能超过最大长度限制 DIV_N
	// 		// witness[i] = witness[i] % (((PR - 1)/2) + 1);
	// 		witness[i] = witness[i] % PR;
	// 	}
	// 	x[i] = IntFp(witness[i], ALICE);
	// }
	// ZKrSqrt(x, y, dim, iter);
	// /****************************/
	// /**** verify correctness ****/
	// /****************************/
	// uint64_t total_err_fixed = 0;
    // uint64_t max_ULP_err_fixed = 0;
	// if (party == ALICE){
	// 	for (int i = 0; i < dim; i++){
	// 		uint64_t k = floor(log2(int64_t(witness[i])));
	// 		uint64_t z = witness[i] * (1ULL << (SQRT_N - 1- k));
	// 		uint64_t k0 = k & 1ULL;  
	// 		uint64_t z1 = (z >> (SQRT_N - 1 - SQRT_M)) & ((1ULL << SQRT_M) - 1);
	// 		// 查表
	// 		double z1bar = 1 + (double(z1) / double(1ULL << SQRT_M));
	// 		double t_real = 1/sqrt((double)((k0 + 1) * z1bar)); 
	// 		uint64_t t = t_real * (1ULL << SCALE);
	// 		// 迭代
	// 		uint64_t a = ((k0 + 1) * z) >> (SQRT_N - 1- SCALE);
	// 		uint64_t b = t;
	// 		uint64_t c = b;
	// 		for (int j = 0; j < iter; j++){
	// 			a = (b * b * a) >> (2 * SCALE);
	// 			b = 3 * (1ULL << SCALE) - a;
	// 			c = (c * b) >> (SCALE + 1);
	// 		}
	// 		double tmp = floor((double)(SCALE + 1 - (int)k)/2.0) + (double)(SQRT_N - SCALE)/2.0;
	// 		uint64_t extendlen = tmp;
	// 		uint64_t ori_y = (c * (1ULL << extendlen)) >> ((SQRT_N - SCALE)/2);
	// 		uint64_t nm_y = (uint64_t)HIGH64(y[i].value);
	// 		if (ori_y != nm_y){
	// 			cout << "rSqrt fault !!!  " << "k: " << k << ", tmp: " << tmp << ", real: " << ori_y << ", protocol: " << nm_y << endl;
	// 		}
	// 		/****************************/
	// 		/****** verify ULPerror *****/
	// 		/****************************/
	// 		double witness_real = Field2Real(witness[i], SCALE);
	// 		double div = 1.0/sqrt(witness_real);
	// 		uint64_t div_field = Real2Field(div, SCALE);
	// 		uint64_t err_fixed = computeULPErr(ori_y, div_field);
    //   		if (err_fixed > 1)
    //   		{
    //     		cout << "ZKsqrt ULP Error Fixed: " << ori_y << "," << div_field << ","
    //          		<< err_fixed << endl;
    //   		}
    //   		total_err_fixed += err_fixed;
    //   		max_ULP_err_fixed = std::max(max_ULP_err_fixed, err_fixed);
	// 	}
	// 	cout << "Average ULP error fixed: " << total_err_fixed / dim << endl;
    // 	cout << "Total ULP error fixed: " << total_err_fixed << endl;
    // 	cout << "Max ULP error fixed: " << max_ULP_err_fixed << endl;
    // 	cout << "Number of tests fixed: " << dim << endl;
	// }

	// cout << "ZKExtend test" << endl;
	// IntFp *x = new IntFp[dim];
	// IntFp *y = new IntFp[dim];
	// IntFp *k = new IntFp[dim];
	// uint64_t *ori_k = new uint64_t[dim]; 
	// for (int i = 0; i < dim; i++){
	// 	uint64_t witness_len = 0;
	// 	ori_k[i] = 0;
	// 	if (party == ALICE){
	// 		witness_len = (i + 2) % (BIT_LENGTH - 1);
	// 		witness[i] = rand() % (1ULL << witness_len);
	// 		witness[i] = rand() % PR;
		
	// 		ori_k[i] = (i + 3) % (BIT_LENGTH - 1);
	// 		if (ori_k[i] + witness_len > (BIT_LENGTH - 1)){
	// 			ori_k[i] = (BIT_LENGTH - 1) - witness_len;
	// 		}
	// 	}
	// 	x[i] = IntFp(witness[i], ALICE);
	// 	k[i] = IntFp(ori_k[i], ALICE);
	// }
	// ZKExtend(x, k, y, dim);
	// /****************************/
	// /**** verify correctness ****/
	// /****************************/
	// if (party == ALICE){
	// 	for (int i = 0; i < dim; i++){
	// 		uint64_t ori_y = witness[i] * (1ULL << ori_k[i]);
	// 		uint64_t nm_y = (uint64_t)HIGH64(y[i].value);
	// 		if (ori_y != nm_y){
	// 			cout << "fault !!!" << endl;
	// 		}
	// 	}
	// }


	endComputation(party);

	cout << "finish test" << endl;

	finalize_zk_bool<BoolIO<NetIO>>();
	finalize_zk_arith<BoolIO<NetIO>>();

	for (int i = 0; i < threads; i++)
	{
		delete ios[i]->io;
		delete ios[i];
	}
	return 0;
}
