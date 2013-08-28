/*
 * Copyright (c) 2013 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_TESTS_TOOLCHAIN_EH_SEPARATE_FILES_H_
#define NATIVE_CLIENT_TESTS_TOOLCHAIN_EH_SEPARATE_FILES_H_ 1

#include <stdint.h>

/*
 * Declare some structs that we will use to do a basic test
 * that pass by value turns into pass by pointer for PNaCl LE32 bitcode.
 */

union VarValue {
  int32_t as_bool;
  int32_t as_int;
  double as_double;
  int64_t as_id;
};

struct Var {
  int32_t type;
  int32_t padding;
  union VarValue value;
};

#endif  // NATIVE_CLIENT_TESTS_TOOLCHAIN_EH_SEPARATE_FILES_H_
