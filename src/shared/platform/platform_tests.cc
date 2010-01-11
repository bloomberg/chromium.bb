/*
 * Copyright 2010 The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


#include <stdio.h>

#include "native_client/src/include/portability.h"
#include "native_client/src/include/checked_cast.h"

#if NACL_WINDOWS
extern "C" {
  int TestFFS();
}
#endif

using nacl::saturate_cast;
using nacl::checked_cast_fatal;

int TestCheckedCastSaturate() {
  int errors = 0;

  uint32_t u32 = 0xffffffff;
  int32_t i32 = -0x12345678;

  uint8_t u8;
  int8_t  i8;

  u8 = saturate_cast<uint8_t>(u32);
  if (u8 != 255) { ++errors; }

  u8 = saturate_cast<uint8_t>(i32);
  if (u8 != 0) { ++errors; }

  i8 = saturate_cast<int8_t>(u32);
  if (i8 != 127) { ++errors; }

  i8 = saturate_cast<int8_t>(i32);
  if (i8 != -128) { ++errors; }

  return errors;
}

int TestCheckedCastFatal() {
  int errors = 0;

  uint32_t u32 = 0xffffffff;
  int32_t i32 = -0x12345678;

  uint8_t u8;
  int8_t  i8;

  u8 = checked_cast_fatal<uint8_t>(u32);
  if (u8 != 255) { ++errors; }

  u8 = checked_cast_fatal<uint8_t>(i32);
  if (u8 != 0) { ++errors; }

  i8 = checked_cast_fatal<int8_t>(u32);
  if (u8 != 127) { ++errors; }

  i8 = checked_cast_fatal<int8_t>(i32);
  if (u8 != -128) { ++errors; }

  return errors;
}

template<int N>
class intN_t {
 public:
  static const int bits = N;
  static const int max = ((1 << (bits - 1)) - 1);
  static const int min = -max - 1;

  intN_t() : storage_(0), overflow_(0) {}

  explicit intN_t(int32_t v) {
    if (v > max || v < min) {
      overflow_ = 1;
    } else {
      overflow_ = 0;
    }
    storage_ = v;
  }

  operator int() const { return storage_; }

  bool Overflow() const { return !!overflow_; }

 private:
  int32_t overflow_  : 1;
  int32_t storage_   : bits;
};

typedef intN_t<28> int28_t;

namespace std {
  template<> struct numeric_limits<int28_t>
  : public numeric_limits<int> {
    static const bool is_specialized = true;
    static int28_t max() {return int28_t(int28_t::max);}
    static int28_t min() {return int28_t(int28_t::min);}
  };
}  // namespace std

int TestCheckedCastUDT() {
  int32_t i32 = 0xf0000000;
  uint32_t u32 = 0xffffffff;

  int errors = 0;

  int28_t i28 = int28_t(0xffffffff);
  if (i28.Overflow()) { ++errors; }

  i28 = int28_t(0x80000000);
  if (!i28.Overflow()) { ++errors; }

  i28 = saturate_cast<int28_t>(i32);
  if (i28.Overflow() || i28 != 0xf8000000) { ++errors; }

  i28 = saturate_cast<int28_t>(u32);
  if (i28.Overflow() || i28 != 0x07ffffff) { ++errors; }

  return errors;
};

/******************************************************************************
 * main
 *****************************************************************************/
int main(int ac,
         char **av) {
  int errors = 0;

  errors += TestCheckedCastSaturate();
  errors += TestCheckedCastUDT();

#if NACL_WINDOWS
  // this test ensures the validity of the assembly version of ffs()
  errors += TestFFS();
#endif

  return errors;
}

