/*
 * Copyright (c) 2013 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/tests/toolchain/eh_separate_files.h"

#include "native_client/tests/toolchain/eh_helper.h"

// Test that even with the non-stable exception handling PNaCl feature
// turned on, we generate the right calling conventions for
// portable (LE32) PNaCl bitcode.  Otherwise, this is like eh_catch_many.cc.

// Declare the functions from the file2 as if it was pass-by-value.
// However, the definition in file2 will actually be pass-by-pointer,
// because that is what PNaCl expands it into.

// We need extern "C" since the prototypes aren't going to match.
extern "C" {
extern __attribute__((noinline))
struct Var return_a_struct(int32_t set_fields_to);

extern __attribute__((noinline))
struct Var call_and_return(struct Var v);

extern __attribute__((noinline))
void call_with_struct(int32_t expected_values, struct Var v);

// file2 will call us back with a pointer, but we call it by-value.
extern __attribute__((noinline))
void call_with_callback(void (*fp)(struct Var* v), struct Var set_fields_to);
}  // extern "C"

class A {
 public:
  A() { next_step(4); }
  ~A() { next_step(10); }
};


class B {
 public:
  B() { next_step(9); }
  ~B() { next_step(12);}
};


class C {
 public:
  C() { abort(); }
  ~C() { abort(); }
};

struct Var global_v;

void my_callback(struct Var* v) {
  global_v = *v;
}

void inner() {
  next_step(3);
  A a;
  try {
    next_step(5);
    int32_t kExpected = (1 << 20) - 1;
    struct Var v = return_a_struct(kExpected);
    if (v.type != kExpected
        || v.value.as_double != (double)(kExpected)) {
      printf("ERROR: return_a_struct mismatch: %d vs %d and %f vs %f\n",
             v.type, kExpected, v.value.as_double, (double)kExpected);
      abort();
    }
    next_step(6);
    v = call_and_return(v);
    if (v.type != kExpected
        || v.value.as_double != (double)(kExpected)) {
      printf("ERROR: call_and_return mismatch: %d vs %d and %f vs %f\n",
             v.type, kExpected, v.value.as_double, (double)kExpected);
      abort();
    }
    next_step(7);
    call_with_struct(kExpected, v);
    // Also test callbacks.
    call_with_callback(&my_callback, v);
    if (global_v.type != kExpected
        || global_v.value.as_double != (double)(kExpected)) {
      printf("ERROR: callback mismatch: %d vs %d and %f vs %f\n",
             global_v.type, kExpected,
             global_v.value.as_double, (double)kExpected);
      abort();
    }
    next_step(8);
    throw B();
    abort();
  } catch(C &) {
    abort();
  }
}


int main() {
  next_step(1);

  try {
    next_step(2);
    inner();
  } catch(...) {
    next_step(11);
  }
  next_step(13);

  return 55;
}
