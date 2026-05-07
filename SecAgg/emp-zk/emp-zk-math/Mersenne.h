#pragma once

#include <x86intrin.h>
#include "gmp.h"
typedef unsigned char byte;

#include <sstream>
#include <string>
#include <vector>

#include "emp-zk/emp-vole/utility.h"  

class ZpMersenneIntElement {
  // private:
public:  // TODO return to private after tesing
  // static const unsigned int p = 2147483647;
  // unsigned int elem;
  uint64_t elem;

public:
  ZpMersenneIntElement() { elem = 0; };
  ZpMersenneIntElement(int64_t elem) {
    this->elem = elem;
    if (this->elem < PR) {
      return;
    }
    this->elem -= PR;
    if (this->elem < PR) {
      return;
    }
    this->elem -= PR;
  }

  ZpMersenneIntElement& operator=(const ZpMersenneIntElement& other) {
    elem = other.elem;
    return *this;
  };

  bool operator!=(const ZpMersenneIntElement& other) { return !(other.elem == elem); };

  ZpMersenneIntElement operator+(const ZpMersenneIntElement& f2) {
    ZpMersenneIntElement answer;

    answer.elem = (elem + f2.elem);

    if (answer.elem >= PR) answer.elem -= PR;

    return answer;
  }
  ZpMersenneIntElement operator-(const ZpMersenneIntElement& f2) {
    ZpMersenneIntElement answer;

    int64_t temp = (int64_t)elem - (int64_t)f2.elem;

    if (temp < 0) {
      answer.elem = temp + PR;
    } else {
      answer.elem = temp;
    }

    return answer;
  }
  ZpMersenneIntElement operator/(const ZpMersenneIntElement& f2) {
    // code taken from NTL for the function XGCD
    int64_t a = f2.elem;
    int64_t b = PR;
    __int128_t s;

    int64_t u, v, q, r;
    __int128_t u0, v0, u1, v1, u2, v2;

    int64_t aneg = 0;

    if (a < 0) {
    //   if (a < -NTL_MAX_LONG) Error("XGCD: integer overflow");
      a = -a;
      aneg = 1;
    }

    if (b < 0) {
    //   if (b < -NTL_MAX_LONG) Error("XGCD: integer overflow");
      b = -b;
    }

    u1 = 1;
    v1 = 0;
    u2 = 0;
    v2 = 1;
    u = a;
    v = b;

    while (v != 0) {
      q = u / v;
      r = u % v;
      u = v;
      v = r;
      u0 = u2;
      v0 = v2;
      u2 = u1 - q * u2;
      v2 = v1 - q * v2;
      u1 = u0;
      v1 = v0;
    }

    if (aneg) u1 = -u1;

    s = u1;

    if (s < 0) s = s + PR;

    ZpMersenneIntElement inverse(s);

    return inverse * (*this);
  }

  ZpMersenneIntElement operator*(const ZpMersenneIntElement& f2) {
    ZpMersenneIntElement answer;

    __int128_t multLong = (__int128_t)elem * (__int128_t)f2.elem;

    // get the bottom 31 bit
    uint64_t bottom = multLong & PR;

    // get the top 31 bits
    uint64_t top = (multLong >> 61);

    answer.elem = bottom + top;

    // maximim the value of 2p-2
    if (answer.elem >= PR) answer.elem -= PR;

    // return ZpMersenneIntElement((bottom + top) %p);
    return answer;
  }

  ZpMersenneIntElement& operator+=(const ZpMersenneIntElement& f2) {
    elem = (f2.elem + elem) % PR;
    return *this;
  };
  ZpMersenneIntElement& operator*=(const ZpMersenneIntElement& f2) {
    __int128_t multLong = (__int128_t)elem * (__int128_t)f2.elem;

    // get the bottom 31 bit
    uint64_t bottom = multLong & PR;

    // get the top 31 bits
    uint64_t top = (multLong >> 61); 
    elem = bottom + top;

    // maximim the value of 2p-2
    if (elem >= PR) elem -= PR;

    return *this;
  }
};

inline std::ostream& operator<<(std::ostream& s, const ZpMersenneIntElement& a) {
  return s << a.elem;
};