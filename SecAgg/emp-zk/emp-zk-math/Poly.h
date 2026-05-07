#pragma once

#include <omp.h>
#include "Mersenne.h"

#include "emp-zk/emp-zk-arith/int_fp.h"

class Poly {
public:
    uint64_t step = 0;
    uint64_t BatchSize = 100;  

    vector<IntFp> checkZero;

    void evalMersenne(ZpMersenneIntElement &Y, 
                        const std::vector<ZpMersenneIntElement> &coeff, 
                        ZpMersenneIntElement X)
    {
        ZpMersenneIntElement acc(0);
        
        for (int64_t i = coeff.size() - 1; i >= 0; i--) {
            acc = acc * X;         // mul(acc, acc, a);
            acc = acc + coeff.at(i);  // add(acc, acc, f.rep[i]);
            //        cout << "coef["<< i << "]: " << coeff[i] << endl;
        }
        
        Y = acc;
    }

    void IntFp_evalMersenne(IntFp &Y, 
                        const std::vector<ZpMersenneIntElement> &coeff, 
                        IntFp X)
    {
        Y = IntFp(coeff.at(coeff.size() - 1).elem, PUBLIC);

        for (int64_t i = coeff.size() - 2; i >= 0; i--) {
            Y = Y * X;    
            Y = Y + coeff.at(i).elem;  
        }

        checkZero.push_back(Y);
        step++;
        
        if (step == BatchSize){
            bool cheat = batch_reveal_check_zero(checkZero.data(), checkZero.size());
            step = 0;
            checkZero.resize(0);
            if (!cheat){
                error("batch check zero fails");
            }
        }
    }

    void interpolateMersenne(std::vector<ZpMersenneIntElement> &coeff,
                                  const std::vector<ZpMersenneIntElement> &X,
                                  std::vector<ZpMersenneIntElement> &Y)
    {
        int64_t m = X.size();
        //   if (Y.size() != X.size()) std::cout << "interpolate: vector length mismatch" << std::endl;
        ZpMersenneIntElement one(1);
        ZpMersenneIntElement zero(0);
        ZpMersenneIntElement p(PR);
        std::vector<ZpMersenneIntElement> prod;
        
        prod = X;
        
        ZpMersenneIntElement t1, t2;

        int64_t k, i;

        std::vector<ZpMersenneIntElement> res;
        res.resize(m);

        for (k = 0; k < m; k++) {
            const ZpMersenneIntElement& aa = X[k];

            t1 = 1;
            for (i = k - 1; i >= 0; i--) {
                t1 = t1 * aa;       // mul(t1, t1, aa);
                t1 = t1 + prod[i];  // add(t1, t1, prod[i]);
            }

            t2 = 0;  // clear(t2);
            for (i = k - 1; i >= 0; i--) {
                t2 = t2 * aa;      // mul(t2, t2, aa);
                t2 = t2 + res[i];  // add(t2, t2, res[i]);
            }

            t1 = one / t1;   // inv(t1, t1);
            t2 = Y[k] - t2;  // sub(t2, b[k], t2);
            t1 = t1 * t2;    // mul(t1, t1, t2);

            for (i = 0; i < k; i++) {
                t2 = prod[i] * t1;     // mul(t2, prod[i], t1);
                res[i] = res[i] + t2;  // add(res[i], res[i], t2);
            }

            res[k] = t1;

            if (k < m - 1) {
                if (k == 0)
                    prod[0] = p - prod[0];  // sub(prod[0], to_ZZ_p(ZZ_pInfo->p),prod[0]);//sub(prod[0],
                                // ZZ_p::modulus(), prod[0]);//negate(prod[0], prod[0]);
                else {
                    t1 = p - X[k];               // sub(t1, to_ZZ_p(ZZ_pInfo->p),a[k]);//negate(t1, a[k]);
                    prod[k] = t1 + prod[k - 1];  // add(prod[k], t1, prod[k-1]);
                    for (i = k - 1; i >= 1; i--) {
                        t2 = prod[i] * t1;           // mul(t2, prod[i], t1);
                        prod[i] = t2 + prod[i - 1];  // add(prod[i], t2, prod[i-1]);
                    }
                    prod[0] = prod[0] * t1;  // mul(prod[0], prod[0], t1);
                }
            }
        } 

        while (m > 0 && !(res[m - 1] != zero)) m--;
        res.resize(m);

        coeff = res;
    }                    
};