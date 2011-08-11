/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * This test ensures that the backends can deal with large list of
 * math intrinsics.
 * This allows nexes to make use of hardware accelaration where available.
 * NOTE: we already use this to accelerate "sqrt" c.f.
 * newlib-trunk/newlib/libm/machine/pnacl/w_sqrt.c
 * which is somewhat problematic because we would get infinite recursion
 * if a backend expands llvm.sqrt.f64 to a call to sqrt()
 */

#include <stdio.h>
#include "native_client/tests/toolchain/utils.h"

/*
 * Note, we intentionally do not include math.h to avoid any magic that
 * might be pulled in that way.
 */
#define PI    3.1415926535897932384626433832795029
#define PI_2  1.5707963267948966192313216916397514
#define E     2.7182818284590452353602874713526625
volatile float f32[] =  {5.0, 16.0, 10.0, PI, PI_2, E };
volatile double f64[] = {5.0, 16.0, 10.0, PI, PI_2, E };

volatile float base32 = 2.0;
volatile double base64 = 2.0;
/*
 * c.f. for a list of these
 * llvm-trunk/lib/CodeGen/SelectionDAG/TargetLowering.cpp
 */
double llvm_intrinsic_sqrt(double) asm("llvm.sqrt.f64");
float llvm_intrinsic_sqrtf(float) asm("llvm.sqrt.f32");

double llvm_intrinsic_log(double) asm("llvm.log.f64");
float llvm_intrinsic_logf(float) asm("llvm.log.f32");

double llvm_intrinsic_log2(double) asm("llvm.log2.f64");
float llvm_intrinsic_log2f(float) asm("llvm.log2.f32");

double llvm_intrinsic_log10(double) asm("llvm.log10.f64");
float llvm_intrinsic_log10f(float) asm("llvm.log10.f32");

double llvm_intrinsic_exp(double) asm("llvm.exp.f64");
float llvm_intrinsic_expf(float) asm("llvm.exp.f32");

double llvm_intrinsic_exp2(double) asm("llvm.exp2.f64");
float llvm_intrinsic_exp2f(float) asm("llvm.exp2.f32");

double llvm_intrinsic_sin(double) asm("llvm.sin.f64");
float llvm_intrinsic_sinf(float) asm("llvm.sin.f32");

double llvm_intrinsic_cos(double) asm("llvm.cos.f64");
float llvm_intrinsic_cosf(float) asm("llvm.cos.f32");

double llvm_intrinsic_pow(double, double) asm("llvm.pow.f64");
float llvm_intrinsic_powf(float, float) asm("llvm.pow.f32");

#define print_op1(op, x) \
  printf("%s: %.6f\n", #op, llvm_intrinsic_ ## op(x))
#define print_op2(op, x, y) \
  printf("%s: %.6f\n", #op, llvm_intrinsic_ ## op(x, y))

int main(int argc, char* argv[]) {
  int i;
  for (i = 0; i < ARRAY_SIZE_UNSAFE(f32); ++i) {
    printf("\nf32 value is: %.6f\n",  f32[i]);
    print_op1(sqrtf, f32[i]);
    print_op1(logf, f32[i]);
    print_op1(log2f, f32[i]);
    print_op1(log10f, f32[i]);
    print_op1(expf, f32[i]);
    print_op1(exp2f, f32[i]);
    print_op1(sinf, f32[i]);
    print_op1(cosf, f32[i]);
    print_op2(powf, base32, f32[i]);
  }

  for (i = 0; i < ARRAY_SIZE_UNSAFE(f64); ++i) {
    printf("\nf64 value is: %.6f\n",  f64[i]);
    print_op1(sqrt, f64[i]);
    print_op1(log, f64[i]);
    print_op1(log2, f64[i]);
    print_op1(log10, f64[i]);
    print_op1(exp, f64[i]);
    print_op1(exp2, f64[i]);
    print_op1(sin, f64[i]);
    print_op1(cos, f64[i]);
    print_op2(pow, base64, f64[i]);
  }

  return 0;
}
