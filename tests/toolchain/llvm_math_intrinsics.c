/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * This test ensures that the front/middle/backends can deal with the
 * math.h functions which have builtins and llvm intrinsics.
 *
 * There are two categories.
 *
 * (1) Standard uses without resorting to llvm assembly:
 * direct calls to C library function, or the gcc __builtin_sin(), etc.
 *
 * (2) Directly testing llvm.* intrinsics. These we test when we know
 * that they *will* be in bitcode or are useful (e.g., we know hardware
 * acceleration is available).  We currently use "llvm.sqrt.*" within
 * the libm sqrt() function: newlib-trunk/newlib/libm/machine/pnacl/w_sqrt.c
 * The libm sqrt is coded to have error checking and errno setting external
 * to the llvm.sqrt.* call.  Other uses of llvm.sqrt have no guarantees
 * about setting errno. This tests that we don't get infinite recursion
 * from such usage (e.g., if a backend expands llvm.sqrt.f64 within sqrt()
 * back into a call to sqrt()).
 */

#include <math.h>
#include <stdio.h>
#include "native_client/tests/toolchain/utils.h"

/* Volatile to prevent library-call constant folding optimizations. */
volatile float f32[] =  {5.0, 16.0, 10.0, M_PI, M_PI_2, M_E };
volatile double f64[] = {5.0, 16.0, 10.0, M_PI, M_PI_2, M_E };

volatile float base32 = 2.0;
volatile double base64 = 2.0;
/*
 * c.f. for a list of these
 * llvm-trunk/lib/CodeGen/SelectionDAG/TargetLowering.cpp
 */
double llvm_intrinsic_sqrt(double) __asm__("llvm.sqrt.f64");
float llvm_intrinsic_sqrtf(float) __asm__("llvm.sqrt.f32");

double llvm_intrinsic_log(double) __asm__("llvm.log.f64");
float llvm_intrinsic_logf(float) __asm__("llvm.log.f32");

double llvm_intrinsic_log2(double) __asm__("llvm.log2.f64");
float llvm_intrinsic_log2f(float) __asm__("llvm.log2.f32");

double llvm_intrinsic_log10(double) __asm__("llvm.log10.f64");
float llvm_intrinsic_log10f(float) __asm__("llvm.log10.f32");

double llvm_intrinsic_exp(double) __asm__("llvm.exp.f64");
float llvm_intrinsic_expf(float) __asm__("llvm.exp.f32");

double llvm_intrinsic_exp2(double) __asm__("llvm.exp2.f64");
float llvm_intrinsic_exp2f(float) __asm__("llvm.exp2.f32");

double llvm_intrinsic_sin(double) __asm__("llvm.sin.f64");
float llvm_intrinsic_sinf(float) __asm__("llvm.sin.f32");

double llvm_intrinsic_cos(double) __asm__("llvm.cos.f64");
float llvm_intrinsic_cosf(float) __asm__("llvm.cos.f32");

double llvm_intrinsic_pow(double, double) __asm__("llvm.pow.f64");
float llvm_intrinsic_powf(float, float) __asm__("llvm.pow.f32");

#define print_op1(prec, op, x)                                      \
  printf("%s (math.h): %." #prec "f\n", #op, op(x));                \
  printf("%s (builtin): %." #prec "f\n", #op, __builtin_ ## op(x));

#define print_op1_llvm(prec, op, x)                                     \
  printf("%s (llvm): %." #prec "f\n", #op, llvm_intrinsic_ ## op(x))

#define print_op2(prec, op, x, y)                                       \
  printf("%s (math.h): %." #prec "f\n", #op, op(x, y));                 \
  printf("%s (builtin): %." #prec "f\n", #op, __builtin_ ## op(x, y));

#define print_op2_llvm(prec, op, x, y)                                  \
  printf("%s (llvm): %." #prec "f\n", #op, llvm_intrinsic_ ## op(x, y))

int main(int argc, char* argv[]) {
  int i;
  for (i = 0; i < ARRAY_SIZE_UNSAFE(f32); ++i) {
    printf("\nf32 value is: %.6f\n",  f32[i]);
    print_op1(5, sqrtf, f32[i]);
    print_op1_llvm(5, sqrtf, f32[i]);
    print_op1(5, logf, f32[i]);
    print_op1(5, log2f, f32[i]);
    print_op1(5, log10f, f32[i]);
    print_op1(4, expf, f32[i]);
    print_op1(4, exp2f, f32[i]);
    print_op1(5, sinf, f32[i]);
    print_op1(5, cosf, f32[i]);
    print_op2(5, powf, base32, f32[i]);
  }

  for (i = 0; i < ARRAY_SIZE_UNSAFE(f64); ++i) {
    printf("\nf64 value is: %.6f\n",  f64[i]);
    print_op1(6, sqrt, f64[i]);
    print_op1_llvm(6, sqrt, f64[i]);
    print_op1(6, log, f64[i]);
    print_op1(6, log2, f64[i]);
    print_op1(6, log10, f64[i]);
    print_op1(6, exp, f64[i]);
    print_op1(6, exp2, f64[i]);
    print_op1(6, sin, f64[i]);
    print_op1(6, cos, f64[i]);
    print_op2(6, pow, base64, f64[i]);
  }

  return 0;
}
