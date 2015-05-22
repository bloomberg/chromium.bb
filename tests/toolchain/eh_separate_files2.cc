/*
 * Copyright (c) 2013 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/tests/toolchain/eh_separate_files.h"

#include <cstdio>
#include <cstdlib>

// Second file for a test that even with the non-stable exception handling
// PNaCl feature turned on, we generate the right calling conventions for
// portable (LE32) PNaCl bitcode.

// NOTE: in file 1 we declare the functions as if it was pass-by-value.
// However, in file 2 we define it as pass-by-pointer,
// because that is what PNaCl abi-simplification expands pass-by-value into.

// We need extern "C" since the prototypes aren't going to match.
// We need __attribute__((noinline) to actually test calling conventions.
extern "C" {
void __attribute__((noinline))
return_a_struct(struct Var* result, int32_t set_fields_to);

void __attribute__((noinline))
call_and_return(struct Var* result, struct Var* v);

void __attribute__((noinline))
call_with_struct(int32_t expected_values, struct Var* v);

// We call the callback as if it is by-value on this side,
// and the input is by pointer.
void __attribute__((noinline))
call_with_callback(void (*fp)(struct Var v), struct Var* set_fields_to);
}  // extern "C"


namespace {

// Check that struct |v| has the |expected_values| in its fields.
// Throw an exception, just to test exceptional control flow a little more.
void check_and_throw(const char* label, int expected_values, struct Var v) {
  if (v.type != expected_values
      || v.value.as_double != (double)(expected_values)) {
    printf("ERROR: check %s mismatch: %d vs %d and %f vs %f\n",
           label, v.type, expected_values,
           v.value.as_double, (double)expected_values);
    abort();
  }
  throw(0);
}

}  // namespace


void return_a_struct(struct Var* result, int32_t set_fields_to) {
  try {
    result->type = set_fields_to;
    result->padding = 0;
    result->value.as_double = (double)set_fields_to;
    check_and_throw("return_a_struct", set_fields_to, *result);
  } catch(int x) {
    if (x != 0)
      abort();
  }
}

void call_and_return(struct Var* result, struct Var* v) {
  try {
    result->type = v->type;
    result->padding = v->padding;
    result->value.as_double = v->value.as_double;
    check_and_throw("call_and_return", v->type, *result);
  } catch(int x) {
    if (x != 0)
      abort();
  }
}

void call_with_struct(int32_t expected_values, struct Var* v) {
  try {
    check_and_throw("call_with_struct", expected_values, *v);
  } catch(int x) {
    if (x != 0)
      abort();
  }
}

void call_with_callback(void (*fp)(struct Var v), struct Var* set_fields_to) {
  try {
    (*fp)(*set_fields_to);
  } catch(int x) {
    if (x != 0)
      abort();
  }
}
