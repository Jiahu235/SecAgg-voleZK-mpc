#include "emp-zk/emp-zk-math/ZKmath-functions.h"

void ZKPositiveDigDec(int party, IntFp *x, IntFp *y, uint64_t *digit_size, int num_digits, int dim)  // x:dim；y:num_digits*dim; num_digits:子串数，digit_size:每个子串的比特长度 digit_size:num_digits
{
	uint64_t curr_x = 0;
	uint64_t ori_y = 0;
	IntFp *sum_ydigits = new IntFp[dim];  // 生成sum_ydigits是为了进行Prove时使用
	vector<IntFp> zero;

	// step1: compute y + check range
	for (int i = 0; i < dim; i++){
		// step 1: 数字分解最低位
		int shiftlen = 0;
		uint64_t curr_digitlen = digit_size[0];
		uint64_t digit_mask = (1ULL << curr_digitlen) - 1;
		if (party == ALICE){
			curr_x = (uint64_t)HIGH64(x[i].value);
			ori_y = (curr_x >> shiftlen) & digit_mask; // 此处先放了低位子串
		}
		sum_ydigits[i] = IntFp(ori_y, ALICE);
		y[i * num_digits] = sum_ydigits[i]; 

		// step 2: check 分解出的最低位的 range
		if (curr_digitlen > 1 && curr_digitlen < NUM_RANGE){
			LUTRange[curr_digitlen]->LUTRangeread(y[i * num_digits]);
		} else if (curr_digitlen == 1){
			IntFp r = (y[i * num_digits].negate() + 1) * y[i * num_digits];
			zero.push_back(r);
		} else {
			error("Incorrect decomposition length");
		}

		for (int j = 1; j < num_digits; j++){
			// step 3: 数字分解其余位
			curr_digitlen = digit_size[j];
			digit_mask = (1ULL << curr_digitlen) - 1;
			shiftlen += digit_size[j - 1];
			if (party == ALICE){
				ori_y = (curr_x >> shiftlen) & digit_mask; // 此处先放了低位子串
			}
			IntFp y_digit = IntFp(ori_y, ALICE);
			y[i * num_digits + j] = y_digit; 

			// step 4: checkrange
			if (curr_digitlen > 1 && curr_digitlen < NUM_RANGE){
				LUTRange[curr_digitlen]->LUTRangeread(y_digit);
			} else if (curr_digitlen == 1){
				IntFp r = (y_digit.negate() + 1) * y_digit;
				zero.push_back(r);
			} else {
				error("Incorrect decomposition length");
			}

			// step 4: 求和
			sum_ydigits[i] = sum_ydigits[i] + y_digit * (1ULL << shiftlen);
		}

		// step 5: checkzero
		IntFp z = x[i] + sum_ydigits[i].negate();
		zero.push_back(z);

		// if (party == ALICE){
		// 	uint64_t x_field = (uint64_t)HIGH64(x[i].value);
		// 	uint64_t sum_field = (uint64_t)HIGH64(sum_ydigits[i].value);
		// 	uint64_t z_field = (uint64_t)HIGH64(z.value);
		// 	if (z_field != 0){
		// 		cout << "x = " << x_field << endl;
		// 		cout << "sum = " << sum_field << endl;
		// 		cout << "z = " << z_field << endl;
		// 	}
		// }
	}

	bool res = batch_reveal_check_zero(zero.data(), zero.size());
	if (!res)
		error("step 2: batch_reveal_check_zero failed");

	delete[] sum_ydigits;
}

void ZKPositiveDigDecAny(int party, IntFp *x, IntFp *xhigh, IntFp *xlow, uint64_t highsize, uint64_t lowsize, int dim)    // 分解成任意长度 x: dim; xhigh: dim; ylow: dim
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
	ZKPositiveDigDec(party, x, y_digits, digit_size, num_digits, dim);

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

void ZKgeneralDigDecAny(int party, IntFp *x, IntFp *y, uint64_t *digit_size, int num_digits, int dim)
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
	ZKPositiveDigDec(party, beforeDigdec, tmp_y, digit_resize.data(), renum_digits, dim);

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

void ZKpositiveTruncAny(int party, IntFp *x, IntFp *y, int dim, uint64_t trunclen)   // 截断任意长度
{
	uint64_t m = ceil(log2(PR)) - 1;
	IntFp *ylow = new IntFp[dim];
	ZKPositiveDigDecAny(party, x, y, ylow, m - trunclen, trunclen, dim);

	delete[] ylow;
}

void ZKgeneralTruncAny(int party, IntFp *x, IntFp *y, int dim, uint64_t trunclen)   
{
	// step 1: cmp
	IntFp *b = new IntFp[dim];
	// ZKcmpReal(party, x, b, dim);
	uint64_t constant = (PR + 1)/2;
	ZKcmpPositive(party, x, constant, b, dim);
	// cout << "Trunc - step 1" << endl;

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
	ZKpositiveTruncAny(party, z, zTrunc, dim, trunclen);
	// cout << "Trunc - step 2" << endl;

	// step 3: line 3
	for (int i = 0; i < dim; i++){
		// cout << "i = " << i << endl;
		y[i] = t1[i] * zTrunc[i] + t2[i].negate();
	}
	// cout << "Trunc - step 3" << endl;

	delete[] b;
	delete[] z;
	delete[] zTrunc;
	delete[] t1;
	delete[] t2;
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


// // ZKcmpReal: old version that needs to limit the range of inputs
// void ZKcmpReal(int party, IntFp *x, IntFp *y, int dim)
// {
// 	uint64_t *z = new uint64_t[dim];
// 	uint64_t *y_field = new uint64_t[dim];
// 	vector<IntFp> zero;
// 	// step 2: define digit set
// 	uint64_t digit_len = NUM_RANGE - 1;
// 	uint64_t num_digit = ceil((double)(BIT_LENGTH - 1)/digit_len);
// 	uint64_t last_digit_len = (BIT_LENGTH - 1) - (num_digit - 1) * digit_len;
// 	uint64_t digit_mask1 = (1ULL << digit_len) - 1;
// 	uint64_t digit_mask2 = (1ULL << last_digit_len) - 1;
// 	IntFp *z_digit = new IntFp[num_digit];
// 	uint64_t z_digit_field = 0;
// 	for (int i = 0; i < dim; i++){
// 		if (party == ALICE){
// 			// step 1: generate z and y
// 			uint64_t x_field = (uint64_t)HIGH64(x[i].value);
// 			if (x_field <= (PR - 1)/2){
// 				z[i] = x_field;
// 				y_field[i] = 1;
// 			} else {
// 				z[i] = PR - x_field;
// 				y_field[i] = 0;
// 			}
// 		}
// 		// step 3: plaintext DigDec, then authZK, then checkrange
// 		for (int j = 0; j < num_digit - 1; j++){
// 			if (party == ALICE){
// 				z_digit_field = (z[i] >> (digit_len * j)) & digit_mask1;
// 			}
// 			z_digit[j] = IntFp(z_digit_field, ALICE);
// 			LUTRange[digit_len]->LUTRangeread(z_digit[j]);
// 		}
// 		if (party == ALICE){
// 			z_digit_field = (z[i] >> (digit_len * (num_digit - 1))) & digit_mask2;
// 		}
// 		z_digit[num_digit - 1] = IntFp(z_digit_field, ALICE);
// 		if (last_digit_len > 1){
// 			LUTRange[last_digit_len]->LUTRangeread(z_digit[num_digit - 1]);
// 		} else {
// 			IntFp r = (z_digit[num_digit - 1].negate() + 1) * z_digit[num_digit - 1];
// 			zero.push_back(r);
// 		}
// 		// step 4: generate y, then check y is a bit
// 		y[i] = IntFp(y_field[i], ALICE);
// 		IntFp tmp = y[i] * (y[i].negate() + 1);
// 		zero.push_back(tmp);
// 		// step 5: generat t, then checkzero
// 		IntFp sum = z_digit[0];
// 		for(int j = 1; j < num_digit; j++){
// 			sum = sum + z_digit[j] * (1ULL << (digit_len * j));
// 		}
// 		IntFp t = y[i] * (sum + x[i].negate()) + (y[i].negate() + 1) * (sum + x[i]);
// 		zero.push_back(t);
// 	}
// 	// step 6: batch check zero
// 	bool res = batch_reveal_check_zero(zero.data(), zero.size());
// 	if (!res)
// 		error("batch_reveal_check_zero failed");
// 	delete[] z;
// 	delete[] y_field;
// 	delete[] z_digit;
// }

void ZKcmpRealVrfyPositive(int party, IntFp *x, uint64_t c, IntFp *y, int dim)
{
	assert(c == (PR+1)/2);
	vector<IntFp> zero;

	// step 1: decompose x
	IntFp *x_digit = new IntFp[dim * FINIAL_CMP_LUT_NUM];
	// cout << "FINIAL_CMP_LUT_NUM: " << FINIAL_CMP_LUT_NUM  << endl;
	uint64_t x_field = 0;
	uint64_t digit_field = 0;
	for (int i = 0; i < dim; i++){
		if (party == ALICE){
			x_field = (uint64_t)HIGH64(x[i].value);
		}
		for (int j = 0; j < FINIAL_CMP_LUT_NUM - 1; j++){
			if (party == ALICE){
				digit_field = (x_field >> (j * CMP_DIGIT_LEN)) & ((1ULL << CMP_DIGIT_LEN) - 1ULL);
			}
			x_digit[i * FINIAL_CMP_LUT_NUM + j] = IntFp(digit_field, ALICE);
		}
		if (party == ALICE){
			digit_field = (x_field >> ((FINIAL_CMP_LUT_NUM - 1) * CMP_DIGIT_LEN)) & ((1ULL << CMP_LAST_DIGIT_BITLEN) - 1);
		}
		x_digit[i * FINIAL_CMP_LUT_NUM + (FINIAL_CMP_LUT_NUM - 1)] = IntFp(digit_field, ALICE);
	}

	// step 2: compute t for checkzero
	IntFp *t = new IntFp[dim];
	for (int i = 0; i < dim; i++){
		t[i] = x_digit[i * FINIAL_CMP_LUT_NUM];
		for (int j = 1; j < FINIAL_CMP_LUT_NUM; j++){
			t[i] = t[i] + x_digit[i * FINIAL_CMP_LUT_NUM + j] * (1ULL << (j * CMP_DIGIT_LEN));
		}
		zero.push_back(t[i] + x[i].negate());
	}

	// step 3: decompose c 
	uint64_t *c_digit = new uint64_t[FINIAL_CMP_LUT_NUM];
	for (int i = 0; i < FINIAL_CMP_LUT_NUM - 1; i++){
		c_digit[i] = (c >> (i * CMP_DIGIT_LEN)) & ((1ULL << CMP_DIGIT_LEN) - 1);
	}
	c_digit[FINIAL_CMP_LUT_NUM - 1] = (c >> ((FINIAL_CMP_LUT_NUM - 1) * CMP_DIGIT_LEN)) & ((1ULL << CMP_LAST_DIGIT_BITLEN) - 1);

	// step 4: compute z
	IntFp *z = new IntFp[FINIAL_CMP_LUT_NUM];
	uint64_t eq = 0;
	uint64_t lt = 0;
	uint64_t z_field = 0;
	uint64_t y_field = 0;
	IntFp sum_z;
	for (int i = 0; i < dim; i++){
		for (int j = 0; j < FINIAL_CMP_LUT_NUM; j++){
			// step 4: compute z
			if(party == ALICE){
				digit_field = (uint64_t)HIGH64(x_digit[i * FINIAL_CMP_LUT_NUM + j].value);
				z_field = LUTvrfyCmpLx[j]->writes_value[digit_field];
			}
			z[j] = IntFp(z_field, ALICE);

			// step 5: Lookup - LUTvrfyCmpLx
			LUTvrfyCmpLx[j]->LUTread(x_digit[i * FINIAL_CMP_LUT_NUM + j], z[j]);
		}

		// step 7: compute sum_z
		sum_z = z[0];
		for (int j = 1; j < FINIAL_CMP_LUT_NUM; j++){
			sum_z = sum_z + z[j];
		}

		// step 6: compute y
		if (party == ALICE){
			uint64_t sum_z_field = (uint64_t)HIGH64(sum_z.value);
			for (int j = 0; j < LUTvrfyCmpLy->writes_index.size(); j++){
				if (LUTvrfyCmpLy->writes_index[j] == sum_z_field){
					y_field = LUTvrfyCmpLy->writes_value[j];
					break;
				}
			}
		}
		y[i] = IntFp(y_field, ALICE);

		// step 8: Lookup - LUTvrfyCmpLy
		LUTvrfyCmpLy->LUTDisRead(sum_z, y[i]);

		// step 9: compute y - 1  for checkzero
		zero.push_back(y[i] + (PR - 1));
	}

	// step 10: checkzero
	bool res = batch_reveal_check_zero(zero.data(), zero.size());
	if (!res)
		error("batch_reveal_check_zero failed");

	delete[] x_digit;
	delete[] t;
	delete[] c_digit;
	delete[] z;
}

void ZKcmpRealVrfyP(int party, IntFp *x, uint64_t c, IntFp *y, int dim)
{
	assert(c == PR);
	vector<IntFp> zero;

	// step 1: decompose x
	IntFp *x_digit = new IntFp[dim * FINIAL_CMP_LUT_NUM];
	uint64_t x_field = 0;
	uint64_t digit_field = 0;
	for (int i = 0; i < dim; i++){
		if (party == ALICE){
			x_field = (uint64_t)HIGH64(x[i].value);
		}
		for (int j = 0; j < FINIAL_CMP_LUT_NUM - 1; j++){
			if (party == ALICE){
				digit_field = (x_field >> (j * CMP_DIGIT_LEN)) & ((1ULL << CMP_DIGIT_LEN) - 1ULL);
			}
			x_digit[i * FINIAL_CMP_LUT_NUM + j] = IntFp(digit_field, ALICE);
		}
		if (party == ALICE){
			digit_field = (x_field >> ((FINIAL_CMP_LUT_NUM - 1) * CMP_DIGIT_LEN)) & ((1ULL << CMP_LAST_DIGIT_BITLEN) - 1);
		}
		x_digit[i * FINIAL_CMP_LUT_NUM + (FINIAL_CMP_LUT_NUM - 1)] = IntFp(digit_field, ALICE);
	}

	// step 2: compute t for checkzero
	IntFp *t = new IntFp[dim];
	for (int i = 0; i < dim; i++){
		t[i] = x_digit[i * FINIAL_CMP_LUT_NUM];
		for (int j = 1; j < FINIAL_CMP_LUT_NUM; j++){
			t[i] = t[i] + x_digit[i * FINIAL_CMP_LUT_NUM + j] * (1ULL << (j * CMP_DIGIT_LEN));
		}
		zero.push_back(t[i] + x[i].negate());
	}

	// step 3: decompose c 
	uint64_t *c_digit = new uint64_t[FINIAL_CMP_LUT_NUM];
	for (int i = 0; i < FINIAL_CMP_LUT_NUM - 1; i++){
		c_digit[i] = (c >> (i * CMP_DIGIT_LEN)) & ((1ULL << CMP_DIGIT_LEN) - 1);
	}
	c_digit[FINIAL_CMP_LUT_NUM - 1] = (c >> ((FINIAL_CMP_LUT_NUM - 1) * CMP_DIGIT_LEN)) & ((1ULL << CMP_LAST_DIGIT_BITLEN) - 1);

	// step 4: compute z
	IntFp *z = new IntFp[FINIAL_CMP_LUT_NUM];
	uint64_t eq = 0;
	uint64_t lt = 0;
	uint64_t z_field = 0;
	uint64_t y_field = 0;
	IntFp sum_z;
	for (int i = 0; i < dim; i++){
		for (int j = 0; j < FINIAL_CMP_LUT_NUM; j++){
			// step 4: compute z
			if(party == ALICE){
				digit_field = (uint64_t)HIGH64(x_digit[i * FINIAL_CMP_LUT_NUM + j].value);
				z_field = LUTvrfyCmpLx_InP[j]->writes_value[digit_field];
			}
			z[j] = IntFp(z_field, ALICE);

			// step 5: Lookup - LUTvrfyCmpLx_InP
			LUTvrfyCmpLx_InP[j]->LUTread(x_digit[i * FINIAL_CMP_LUT_NUM + j], z[j]);
		}

		// step 7: compute sum_z
		sum_z = z[0];
		for (int j = 1; j < FINIAL_CMP_LUT_NUM; j++){
			sum_z = sum_z + z[j];
		}

		// step 6: compute y
		if (party == ALICE){
			uint64_t sum_z_field = (uint64_t)HIGH64(sum_z.value);
			for (int j = 0; j < LUTvrfyCmpLy->writes_index.size(); j++){
				if (LUTvrfyCmpLy->writes_index[j] == sum_z_field){
					y_field = LUTvrfyCmpLy->writes_value[j];
					break;
				}
			}
		}
		y[i] = IntFp(y_field, ALICE);

		// step 8: Lookup - LUTvrfyCmpLy
		LUTvrfyCmpLy->LUTDisRead(sum_z, y[i]);

		// step 9: compute y - 1  for checkzero
		zero.push_back(y[i] + (PR - 1));
	}

	// step 10: checkzero
	bool res = batch_reveal_check_zero(zero.data(), zero.size());
	if (!res)
		error("batch_reveal_check_zero failed");

	delete[] x_digit;
	delete[] t;
	delete[] c_digit;
	delete[] z;
}

void ZKcmpPositive(int party, IntFp *x, uint64_t c, IntFp *y, int dim)
{
	assert(c == (PR+1)/2);
	vector<IntFp> zero;

	// step 1: decompose x
	IntFp *x_digit = new IntFp[dim * FINIAL_CMP_LUT_NUM];
	uint64_t x_field = 0;
	uint64_t digit_field = 0;
	for (int i = 0; i < dim; i++){
		if (party == ALICE){
			x_field = (uint64_t)HIGH64(x[i].value);
		}
		for (int j = 0; j < FINIAL_CMP_LUT_NUM - 1; j++){
			if (party == ALICE){
				digit_field = (x_field >> (j * CMP_DIGIT_LEN)) & ((1ULL << CMP_DIGIT_LEN) - 1ULL);
			}
			x_digit[i * FINIAL_CMP_LUT_NUM + j] = IntFp(digit_field, ALICE);
		}
		if (party == ALICE){
			digit_field = (x_field >> ((FINIAL_CMP_LUT_NUM - 1) * CMP_DIGIT_LEN)) & ((1ULL << CMP_LAST_DIGIT_BITLEN) - 1);
		}
		x_digit[i * FINIAL_CMP_LUT_NUM + (FINIAL_CMP_LUT_NUM - 1)] = IntFp(digit_field, ALICE);
	}

	// step 2: compute t for checkzero
	IntFp *t = new IntFp[dim];
	for (int i = 0; i < dim; i++){
		t[i] = x_digit[i * FINIAL_CMP_LUT_NUM];
		for (int j = 1; j < FINIAL_CMP_LUT_NUM; j++){
			t[i] = t[i] + x_digit[i * FINIAL_CMP_LUT_NUM + j] * (1ULL << (j * CMP_DIGIT_LEN));
		}
		zero.push_back(t[i] + x[i].negate());
	}

	// step 3: decompose c 
	uint64_t *c_digit = new uint64_t[FINIAL_CMP_LUT_NUM];
	for (int i = 0; i < FINIAL_CMP_LUT_NUM - 1; i++){
		c_digit[i] = (c >> (i * CMP_DIGIT_LEN)) & ((1ULL << CMP_DIGIT_LEN) - 1);
	}
	c_digit[FINIAL_CMP_LUT_NUM - 1] = (c >> ((FINIAL_CMP_LUT_NUM - 1) * CMP_DIGIT_LEN)) & ((1ULL << CMP_LAST_DIGIT_BITLEN) - 1);

	// step 4: decompose PR 
	uint64_t *p_digit = new uint64_t[FINIAL_CMP_LUT_NUM];
	for (int i = 0; i < FINIAL_CMP_LUT_NUM - 1; i++){
		p_digit[i] = (PR >> (i * CMP_DIGIT_LEN)) & ((1ULL << CMP_DIGIT_LEN) - 1);
	}
	p_digit[FINIAL_CMP_LUT_NUM - 1] = (PR >> ((FINIAL_CMP_LUT_NUM - 1) * CMP_DIGIT_LEN)) & ((1ULL << CMP_LAST_DIGIT_BITLEN) - 1);

	// step 4: compute z and u
	IntFp *z = new IntFp[FINIAL_CMP_LUT_NUM];
	IntFp *u = new IntFp[FINIAL_CMP_LUT_NUM];
	IntFp *v = new IntFp[dim];
	uint64_t eq = 0;
	uint64_t lt = 0;
	uint64_t z_field = 0;
	uint64_t u_field = 0;
	uint64_t y_field = 0;
	uint64_t v_field = 0;
	IntFp sum_z;
	IntFp sum_u;
	for (int i = 0; i < dim; i++){
		for (int j = 0; j < FINIAL_CMP_LUT_NUM; j++){
			// step 4: compute z and u
			if(party == ALICE){
				digit_field = (uint64_t)HIGH64(x_digit[i * FINIAL_CMP_LUT_NUM + j].value);
				z_field = LUTCmpLx[j]->writes_a[digit_field];
				u_field = LUTCmpLx[j]->writes_b[digit_field];
			}
			z[j] = IntFp(z_field, ALICE);
			u[j] = IntFp(u_field, ALICE);

			// step 5: Lookup - LUTvrfyCmpLx_InP
			LUTCmpLx[j]->LUTTwoValueread(x_digit[i * FINIAL_CMP_LUT_NUM + j], z[j], u[j]);
		}

		// step 7: compute sum_z and sum_u
		sum_z = z[0];
		sum_u = u[0];
		for (int j = 1; j < FINIAL_CMP_LUT_NUM; j++){
			sum_z = sum_z + z[j];
			sum_u = sum_u + u[j];
		}

		// step 6: compute y and v
		if (party == ALICE){
			int64_t correct_index_y = -1;
			int64_t correct_index_v = -1;
			uint64_t sum_z_field = (uint64_t)HIGH64(sum_z.value);
			uint64_t sum_u_field = (uint64_t)HIGH64(sum_u.value);
			for (int j = 0; j < LUTvrfyCmpLy->writes_index.size(); j++){
				if (LUTvrfyCmpLy->writes_index[j] == sum_z_field){
					y_field = LUTvrfyCmpLy->writes_value[j];
					correct_index_y = j;
				}
				if (LUTvrfyCmpLy->writes_index[j] == sum_u_field){
					v_field = LUTvrfyCmpLy->writes_value[j];
					correct_index_v = j;
				}
				if (correct_index_y > (-1) &&  correct_index_v > (-1)){
					break;
				}
			}
		}
		y[i] = IntFp(y_field, ALICE);
		v[i] = IntFp(v_field, ALICE);

		// step 8: Lookup - LUTvrfyCmpLy
		LUTvrfyCmpLy->LUTDisRead(sum_z, y[i]);
		LUTvrfyCmpLy->LUTDisRead(sum_u, v[i]);

		// step 9: compute v - 1  for checkzero
		zero.push_back(v[i] + (PR - 1));
	}

	// step 10: checkzero
	bool res = batch_reveal_check_zero(zero.data(), zero.size());
	if (!res)
		error("batch_reveal_check_zero failed");

	delete[] x_digit;
	delete[] t;
	delete[] c_digit;
	delete[] p_digit;
	delete[] z;
	delete[] u;
	delete[] v;
}

// // Max: old version that requires A2B
// void ZKMax(IntFp *x, IntFp *y, int rows, int cols)    // matrix中每一行求一个max; x: rows*cols  y:rows
// {
// 	Integer *x_bool = new Integer[rows * cols];
// 	Integer *y_bool = new Integer[rows];
// 	arith2bool<BoolIO<NetIO>>(x_bool, x, rows * cols);       
// 	for (int i = 0; i < rows; i++){
// 		y_bool[i] = x_bool[i * cols];
// 		for (int j = 1; j < cols; j++){
// 			Integer curr = x_bool[i * cols + j];
// 			Bit res = curr.geq(y_bool[i]);
// 			y_bool[i] = y_bool[i].select(res, curr);
// 		}
// 	}
// 	bool2arith<BoolIO<NetIO>>(y, y_bool, rows);
// 	delete[] x_bool;
// 	delete[] y_bool;
// }

void ZKMax(int party, IntFp *x, IntFp *y, int rows, int cols)    // matrix中每一行求一个max; x: rows*cols  y:rows
{ 
	// step 1: Prover 生成 y
	uint64_t x_max = 0;
	for (int i = 0; i < rows; i++){
		if (party == ALICE){
			x_max = (uint64_t)HIGH64(x[i * cols].value);
			for (int j = 1; j < cols; j++){
				uint64_t x_field = (uint64_t)HIGH64(x[i * cols + j].value);
				// if (x_field > x_max){
				// 	x_max = x_field;
				// }
				// 注意： 此处是真实值的比较。下述情况下会更新x_max的值。当然也会有其他情况，但是其他情况不需要改变x_max的值，故不用写出来
				if ((x_max <= (PR-1)/2) && (x_field <= (PR-1)/2) || (x_max > (PR-1)/2) && (x_field > (PR-1)/2)){
					if (x_field > x_max){
						x_max = x_field;
					}
				} else if ((x_max > (PR-1)/2) && (x_field <= (PR-1)/2)){
					x_max = x_field;
				}
			}
		}
		y[i] = IntFp(x_max, ALICE);
	}

	// step 2: compute t = x_max - x
	IntFp *t = new IntFp[rows * cols];
	for (int i = 0; i < rows; i++){
		for (int j = 0; j < cols; j++){
			t[i * cols + j] = y[i] + x[i * cols + j].negate();
		}
	}

	// step 3: invoke comparison
	IntFp *b = new IntFp[rows * cols];
	// ZKcmpReal(party, t, b, rows * cols);
	uint64_t constant = (PR+1)/2;
	ZKcmpRealVrfyPositive(party, t, constant, b, rows * cols);

	// step 4: multiplication and truncation
	IntFp *d = new IntFp[rows];
	for (int i = 0; i < rows; i++){
		d[i] = t[i * cols];
	}
	for (int i = 1; i < cols; i++){
		for (int j = 0; j < rows; j++){
			d[j] = d[j] * t[j * cols + i];
		}
		// ZKpositiveTruncAny(party, d, d, rows, SCALE);   // 我们是在check不是在计算真实值，所以无需截断，只要有0，不管是否溢出，d必然为0
	}

	// checkzero
	vector<IntFp> zero;
	for (int i = 0; i < rows; i++){
		zero.push_back(d[i]);
		// if (party == ALICE){
		// 	uint64_t d_field = (uint64_t)HIGH64(d[i].value);
		// 	if (d_field != 0){
		// 		cout << "d = " << d_field << endl;
		// 	}
		// }
		for (int j = 0; j < cols; j++){
			zero.push_back(b[i * cols + j] + (PR-1));
		}
	}
	bool res = batch_reveal_check_zero(zero.data(), zero.size());
	if (!res)
		error("batch_reveal_check_zero failed");

	delete[] t;
	delete[] b;
	delete[] d;
}

// // MSNZB: old version that requires A2B
// void ZKmsnzb(int party, IntFp *x, IntFp *y, int dim)
// {
// 	uint64_t msnzb = 0;
// 	uint64_t z = 0;
// 	IntFp *z1 = new IntFp[dim];
// 	// IntFp *z2 = new IntFp[dim];
// 	for (int i = 0; i < dim; i++){
// 		if (party == ALICE){
// 			uint64_t curr_x = (uint64_t)HIGH64(x[i].value);
// 			msnzb = floor(log2(curr_x));   // msnzb: 最高非零位的index (从0开始)
// 			z = 1ULL << msnzb;
// 		}
// 		y[i] = IntFp(msnzb, ALICE);
// 		IntFp value = IntFp(z, ALICE);
//         LUTmsnzb->LUTread(y[i], value);
// 		z1[i] = value;
// 		// z2[i] = value * 2;
// 	}
// 	Integer *b0_bool = new Integer[dim];
// 	Integer *b1_bool = new Integer[dim];
//     // IntFp *b0 = new IntFp[dim];
// 	// IntFp *b1 = new IntFp[dim];
// 	// ZKFpCompareLEQ(z1, x, b0, dim);
// 	// ZKFpCompare(x, z2, b1, dim);   // if x < y output 1
// 	Integer *x_bool = new Integer[dim];
// 	Integer *z1_bool = new Integer[dim];
// 	Integer *z2_bool = new Integer[dim];
// 	arith2bool<BoolIO<NetIO>>(x_bool, x, dim);    
// 	arith2bool<BoolIO<NetIO>>(z1_bool, z1, dim);
// 	Integer one = Integer(62, 1, PUBLIC);
// 	for (int i = 0; i < dim; i++){
// 		// step 1: 计算 b0 = 1{x >= z1}
// 		Integer res = x_bool[i] - z1_bool[i];
// 		b0_bool[i] = (res >> BIT_LENGTH) ^ one;
// 		// step 2: 计算 z2 = 2*z1
// 		z2_bool[i] = z1_bool[i] << 1;
// 		//step 3: 计算 b1 = 1{x < z2}
// 		res = x_bool[i] - z2_bool[i];
// 		b1_bool[i] = res >> BIT_LENGTH;
// 	}
// 	// bool2arith<BoolIO<NetIO>>(b0, b0_bool, dim);
// 	// bool2arith<BoolIO<NetIO>>(b1, b1_bool, dim);
// 	// for (int i = 0; i < dim; i++){
// 	// 	// b0[i] = b0[i] * b1[i] + (PR - 1);
// 	// 	b0[i] = b0[i] + (PR - 1);
// 	// 	b1[i] = b1[i] + (PR - 1);
// 	// }
// 	// bool res = batch_reveal_check_zero(b0, dim);
// 	// if (!res)
// 	// 	error("batch_reveal_check_zero failed");
// 	// res = batch_reveal_check_zero(b1, dim);
// 	// if (!res)
// 	// 	error("batch_reveal_check_zero failed");
// 	bool cheat = true;
// 	for (int i = 0; i < dim; i++){
// 		Bit eq = b0_bool[i].equal(one) & b1_bool[i].equal(one);
// 		bool res = eq.reveal<bool>(PUBLIC);
// 		cheat = cheat and res;
// 	}
// 	if (!cheat)
// 			error("cheat!");
// 	delete[] z1;
// 	delete[] z1_bool;
// 	delete[] z2_bool;
// 	// delete[] b0;
// 	// delete[] b1;
// 	delete[] b0_bool;
// 	delete[] b1_bool;
// }

void ZKmsnzb(int party, IntFp *x, IntFp *y, int dim)
{
	uint64_t msnzb = 0;
	uint64_t z0_field = 0;
	uint64_t z1_field = 0;
	IntFp *z0 = new IntFp[dim];	
	IntFp *z1 = new IntFp[dim];

	// step 1: compute msnzb, then checkLUT
	for (int i = 0; i < dim; i++){
		if (party == ALICE){
			uint64_t curr_x = (uint64_t)HIGH64(x[i].value);
			msnzb = floor(log2(curr_x));   // msnzb: 最高非零位的index (从0开始)
			z0_field = 1ULL << msnzb;
			if (msnzb == (ceil(log2(PR)) - 2)){
				z1_field = (PR - 1)/2;
			} else {
				z1_field = (1ULL << (msnzb + 1)) - 1;
			}
		}
		y[i] = IntFp(msnzb, ALICE);
		z0[i] = IntFp(z0_field, ALICE);
		z1[i] = IntFp(z1_field, ALICE);
        LUTmsnzb2value->LUTTwoValueread(y[i], z0[i], z1[i]);
	}

	// step 2: cmp
	IntFp *b0 = new IntFp[dim];
	IntFp *b1 = new IntFp[dim];
	IntFp *in0 = new IntFp[dim];
	IntFp *in1 = new IntFp[dim];
	for (int i = 0; i < dim; i++){
		in0[i] = x[i] + z0[i].negate();
		in1[i] = z1[i] + x[i].negate();
	}
	// ZKcmpReal(party, in0, b0, dim);
	// ZKcmpReal(party, in1, b1, dim);
	uint64_t constant = (PR+1)/2;
	ZKcmpRealVrfyPositive(party, in0, constant, b0, dim);
	ZKcmpRealVrfyPositive(party, in1, constant, b1, dim);

	// step 3: checkzero
	vector<IntFp> zero;
	for (int i = 0; i < dim; i++){
		zero.push_back(b0[i] + (PR - 1));
		zero.push_back(b1[i] + (PR - 1));
	}

	bool res = batch_reveal_check_zero(zero.data(), zero.size());
	if (!res)
		error("batch_reveal_check_zero failed");

	delete[] z0;
	delete[] z1;
	delete[] b0;
	delete[] b1;
	delete[] in0;
	delete[] in1;
}

void ZKExtend(int party, IntFp *x, IntFp *k, IntFp *y, int dim)    // 用于Div
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

void ZKExtendSqrt(int party, IntFp *x, IntFp *k, IntFp *y, int dim)    // 用于rSqrt
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

void ZKExp(int party, IntFp *x, IntFp *y, int dim)  // 向量中每个元素求指数
{
	// step 1: DigitDec
	IntFp *xDigDec = new IntFp[dim * EXP_LUT_NUM];
	uint64_t *digit_size = new uint64_t[EXP_LUT_NUM];
	// memset(digit_size, EXP_DIGIT_LEN, EXP_LUT_NUM * sizeof(uint64_t));  // 这样写结果不对
	for (int i = 0; i < EXP_LUT_NUM - 1; i++){
		digit_size[i] = EXP_DIGIT_LEN;
	}
	digit_size[EXP_LUT_NUM - 1] = EXP_N - EXP_DIGIT_LEN * (EXP_LUT_NUM - 1);
	ZKPositiveDigDec(party, x, xDigDec, digit_size, EXP_LUT_NUM, dim);

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
		ZKpositiveTruncAny(party, y, y, dim, SCALE);
	}

	delete[] xDigDec;
	delete[] digit_size;
	delete[] yDigDec;
}

void ZKDiv(int party, IntFp *x, IntFp *y, int dim)  //一个元素的倒数
{
	// step 1: msnzb
	IntFp *k = new IntFp[dim];
	ZKmsnzb(party, x, k, dim);
	// cout << "finish step 1" << endl;

	// step 2: extend
	IntFp *extendLen = new IntFp[dim]; 
	for (int i = 0; i < dim; i++){
		extendLen[i] = k[i].negate() + (DIV_N - 1);
	}
	IntFp *z = new IntFp[dim];
	ZKExtend(party, x, extendLen, z, dim);

	// step 3: DigitDec
	for (int i = 0; i < dim; i++){
		z[i] = z[i] + (PR - (1ULL << (DIV_N - 1)));     
	}
	IntFp *z1 = new IntFp[dim];
	IntFp *z0 = new IntFp[dim];
	ZKPositiveDigDecAny(party, z, z1, z0, DIV_M, DIV_N - 1 - DIV_M, dim);

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

	ZKpositiveTruncAny(party, yprime, yprimeTrunc, dim, DIV_N - 1);

	// step 5: extend + trunc
	// for (int i = 0; i < dim; i++){
	// 	extendLen[i] = k[i].negate() + DIV_N;
	// }
	ZKExtend(party, yprimeTrunc, extendLen, yprime, dim);  // 重用了yprime,放置ZKExtend的输出
	ZKpositiveTruncAny(party, yprime, y, dim, DIV_N - SCALE - 1);

	delete[] k;
	delete[] extendLen;
	delete[] z;
	delete[] z1;
	delete[] z0;
	delete[] yprime;
	delete[] yprimeTrunc;
}

void ZKrSqrt(int party, IntFp *x, IntFp *y, int dim, int iter)
{
	// step 1: msnzb
	IntFp *k = new IntFp[dim];
	ZKmsnzb(party, x, k, dim);

	// step 2: extend
	IntFp *extendLen = new IntFp[dim]; 
	for (int i = 0; i < dim; i++){
		extendLen[i] = k[i].negate() + (SQRT_N - 1);
	}
	IntFp *z = new IntFp[dim];
	ZKExtend(party, x, extendLen, z, dim);

	// step 3: DigitDec
	IntFp *k1 = new IntFp[dim];
	IntFp *k0 = new IntFp[dim];
	ZKPositiveDigDecAny(party, k, k1, k0, ceil(log2(SQRT_N)) - 1, 1, dim);
	IntFp *zprime = new IntFp[dim];
	for (int i = 0; i < dim; i++){
		zprime[i] = z[i] + (PR - (1ULL << (SQRT_N - 1)));     
	}
	IntFp *z1 = new IntFp[dim];
	IntFp *z0 = new IntFp[dim];
	ZKPositiveDigDecAny(party, zprime, z1, z0, SQRT_M, SQRT_N - 1 - SQRT_M, dim);

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
	ZKpositiveTruncAny(party, abeforeTrunc, a, dim, SQRT_N - 1 - SCALE);

	// step 6: iteration
	IntFp *cbeforeTrunc = new IntFp[dim];
	for (int i = 0; i < iter; i++){
		for (int j = 0; j < dim; j++){
			abeforeTrunc[j] = b[j] * b[j] * a[j];
		}
		ZKpositiveTruncAny(party, abeforeTrunc, a, dim, 2 * SCALE);
		for (int j = 0; j < dim; j++){
			b[j] = a[j].negate() + (3 * (1ULL << SCALE));  
			cbeforeTrunc[j] = c[j] * b[j];
		}
		ZKpositiveTruncAny(party, cbeforeTrunc, c, dim, SCALE + 1);
	}

	// step 7: extend and truncation
	IntFp *CI = new IntFp[dim];
	ZKExtendSqrt(party, c, k, CI, dim);
	uint64_t trunclen = (SQRT_N - SCALE)/2;
	ZKpositiveTruncAny(party, CI, y, dim, trunclen);

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

void ZKSigmoid(int party, IntFp *x, IntFp *y, int dim)
{
	// step 1: compute whether x is positive 
	IntFp *b = new IntFp[dim];
	uint64_t constant = (PR+1)/2;
	ZKcmpPositive(party, x, constant, b, dim);

	// step 2: compute x_bar
	IntFp *x_bar = new IntFp[dim];
	for (int i = 0; i < dim; i++){
		x_bar[i] = (b[i] * 2 + (PR - 1)) * x[i];
	}

	// step 3: compute z
	IntFp *z = new IntFp[dim];
	ZKExp(party, x_bar, z, dim);

	// step 4: coompute d1
	IntFp *d1 = new IntFp[dim];
	IntFp *zaddone = new IntFp[dim];
	for (int i = 0; i < dim; i++){
		zaddone[i] = z[i] + 1;
	}
	ZKDiv(party, zaddone, d1, dim);

	// step 5: compute d2 and truncation
	IntFp *d2 = new IntFp[dim];
	for (int i = 0; i < dim; i++){
		d2[i] = z[i] * d1[i];
	}
	ZKpositiveTruncAny(party, d2, d2, dim, SCALE);

	// step 6: compute y
	for (int i = 0; i < dim; i++){
		y[i] = b[i] * d1[i] + (b[i].negate() + 1) * d2[i];
	}
}

void ZKGeLU(int party, IntFp *x, IntFp *y, int dim)
{
	// step 1: compute x^3
	IntFp *k = new IntFp[dim];
	for (int i = 0; i < dim; i++){
		k[i] = x[i] * x[i] * x[i];
	}
	ZKgeneralTruncAny(party, k, k, dim, 2 * SCALE);

	// step 2: compute t
	IntFp *t = new IntFp[dim];
	uint64_t a_field = Real2Field(sqrt(2/M_PI), SCALE);
	uint64_t b_field = Real2Field(0.044715, SCALE);
	for (int i = 0; i < dim; i++){
		t[i] = (x[i] + k[i] * b_field) * a_field;
	}
	ZKgeneralTruncAny(party, t, t, dim, 2 * SCALE);

	// step 3: compute c
	IntFp *c = new IntFp[dim];
	for (int i = 0; i < dim; i++){
		c[i] = t[i] * 2;
	}
	ZKgeneralTruncAny(party, c, c, dim, SCALE);

	// step 4: compute sigmoid
	IntFp *d = new IntFp[dim];
	ZKSigmoid(party, c, d, dim);

	// step 5: compute z
	IntFp *z = new IntFp[dim];
	for (int i = 0; i < dim; i++){
		z[i] = d[i] * 2 + (PR - 1);
	}
	ZKgeneralTruncAny(party, z, z, dim, SCALE);

	// step 6: compute y
	uint64_t c_field = Real2Field(0.5, SCALE);
	for (int i = 0; i < dim; i++){
		y[i] = x[i] * (z[i] + 1) * c_field;
	}
	ZKgeneralTruncAny(party, y, y, dim, 2 * SCALE);

	delete[] k;
	delete[] t;
	delete[] c;
	delete[] d;
	delete[] z;
}

void ZKSoftmax(int party, IntFp *x, IntFp *y, int rows, int cols)  // x: rows*cols  y: rows
{
	IntFp *max = new IntFp[rows];
	IntFp *z = new IntFp[rows * cols];
	IntFp *ez = new IntFp[rows * cols];
	IntFp *sum = new IntFp[rows];
	IntFp *t = new IntFp[rows];
	IntFp *ybeforeTrunc = new IntFp[rows * cols];
	
	// step 1: Max: 每一行有一个max
	ZKMax(party, x, max, rows, cols);

	// step 2: exp
	for (int i = 0; i < rows; i++){
		for (int j = 0; j < cols; j++){
			z[i * cols + j] = x[i * cols + j].negate() + max[i];
		}
	}
	ZKExp(party, z, ez, rows * cols);

	// step 3: sum
	for (int i = 0; i < rows; i++){
		sum[i] = ez[i * cols];
		for (int j = 1; j < cols; j++){
			sum[i] = sum[i] + ez[i * cols + j];
		}
	}

	// step 4: div
	ZKDiv(party, sum, t, rows);
	
	// step 4: mult + trunc
	for (int i = 0; i < rows; i++){
		for (int j = 0; j < cols; j ++){
			ybeforeTrunc[i * cols + j] = t[i] * ez[i * cols + j];
		}
	}
	ZKpositiveTruncAny(party, ybeforeTrunc, y, rows * cols, SCALE);

	delete[] max;
	delete[] z;
	delete[] ez;
	delete[] sum;
	delete[] t;
	delete[] ybeforeTrunc;
}

void ZKLayerNorm(int party, IntFp *x, IntFp *y, IntFp *gamma, IntFp *beta, int rows, int cols)  // gamma: rows; beta: rows
{
	// uint64_t cols_field = Real2Field(1.0/cols, SCALE);
	int64_t x_scale = floor((double)(1.0/cols) * (1ULL << SCALE));
    uint64_t cols_field = x_scale < 0 ? PR + x_scale : x_scale; 

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
	ZKgeneralTruncAny(party, mu, mu, rows, SCALE);
	cout << "end step 1" << endl;

	// step 2: compute sigma
	IntFp *sigma = new IntFp[rows];
	IntFp *tmp = new IntFp[rows * cols];
	for (int i = 0; i < rows; i++){
		// IntFp tmp = x[i * cols] + mu[i].negate();
		// sum[i] = tmp * tmp;
		tmp[i * cols] = x[i * cols] + mu[i].negate();;
		sum[i] = tmp[i * cols] * tmp[i * cols];
		for (int j = 1; j < cols; j++){
			// tmp = x[i * cols + j] + mu[i].negate();
			// sum[i] = sum[i] + (tmp * tmp);
			tmp[i * cols + j] = x[i * cols + j] + mu[i].negate();
			sum[i] = sum[i] + (tmp[i * cols + j] * tmp[i * cols + j]);
		}
		sigma[i] = sum[i] * cols_field;
	}
	ZKpositiveTruncAny(party, sigma, sigma, rows, 2 * SCALE);
	cout << "end step 2" << endl;

	// step 3: compute t - sqrt
	IntFp *t = new IntFp[rows];
	ZKrSqrt(party, sigma, t, rows, 1);
	cout << "end step 3" << endl;

	// step 4: compute z
	IntFp *z = new IntFp[rows * cols];
	for (int i = 0; i < rows; i++){
		for (int j = 0; j < cols; j++){
			// z[i * cols + j] = t[i] * (x[i * cols + j] + mu[i].negate());
			z[i * cols + j] = t[i] * tmp[i * cols + j];
		}
	}
	ZKgeneralTruncAny(party, z, z, rows * cols, SCALE);
	cout << "end step 4" << endl;

	// step 5: compute y
	IntFp *ybeforeTrunc = new IntFp[rows * cols];
	for (int i = 0; i < rows; i++){
		for (int j = 0; j < cols; j++){
			ybeforeTrunc[i * cols + j] = z[i * cols + j] * gamma[i] + beta[i];
		}
	}
	cout << "end circle" << endl;
	ZKgeneralTruncAny(party, ybeforeTrunc, y, rows * cols, SCALE);
	cout << "end step 5" << endl;

	delete[] mu;
	delete[] sum;
	delete[] sigma;
	delete[] tmp;
	delete[] t;
	delete[] z;
	delete[] ybeforeTrunc;

	cout << "softmax - finish delete" << endl;
}

