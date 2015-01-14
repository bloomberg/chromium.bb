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
volatile float f32[] = {-NAN, NAN, -INFINITY, -HUGE_VALF,
                        -M_E, -M_PI_2, -M_PI, -16.0, -0.5, -0.0,
                        0.0, 5.0, 16.0, 10.0, M_PI, M_PI_2, M_E,
                        HUGE_VALF, INFINITY };
volatile double f64[] = {-NAN, NAN, -INFINITY, -HUGE_VAL,
                         -M_E, -M_PI_2, -M_PI, -16.0, -0.5, -0.0,
                         0.0, 5.0, 16.0, 10.0, M_PI, M_PI_2, M_E,
                         HUGE_VAL, INFINITY};

volatile float base32 = 2.0;
volatile float neg_base32 = -2.0;
volatile double base64 = 2.0;
volatile double neg_base64 = -2.0;

/*
 * The LLVM language reference considers this undefined for values < -0.0.
 * In practice hardware sqrt instructions returns NaN in that case.
 * We will need to guarantee this behavior.
 */
double llvm_intrinsic_sqrt(double) __asm__("llvm.sqrt.f64");
float llvm_intrinsic_sqrtf(float) __asm__("llvm.sqrt.f32");

/*
 * Normally, printf can end up printing NAN and INFINITY values
 * differently depending on the libc.
 *
 * C99 7.19.6.1 The fprintf function, paragraph 8, section of the f,F
 * modifiers says the following:
 *
 * A double argument representing an infinity is converted in one of
 * the styles [-]inf or [-]infinity — which style is implementation-defined.
 * A double argument representing a NaN is converted in one of the styles
 * [-]nan or [-]nan(n-char-sequence) — which style, and the meaning of
 * any n-char-sequence, is implementation-defined. The F conversion
 * specifier produces INF, INFINITY, or NAN instead of inf, infinity,
 * or nan, respectively.
 *
 * This custom routine works around a newlib bug:
 * https://code.google.com/p/nativeclient/issues/detail?id=4039
 * TODO(jvoung): remove the workaround when newlib is fixed.
 *
 * Also, there are some cases where the intrinsics and functions
 * currently return different values on different architectures
 * (see test cases below). The |nan_sign| parameter should be true
 * if it's okay to print the sign of the nan for the golden output
 * (consistent / no known platform difference), and false otherwise.
 */
void sprint_double(char *buf, const char *format_with_prec,
                   double x, int nan_sign) {
  if (isnan(x)) {
    if (nan_sign && signbit(x) != 0)
      sprintf(buf, "-nan");
    else
      sprintf(buf, "nan");
  } else if (isinf(x)) {
    if (signbit(x) != 0)
      sprintf(buf, "-inf");
    else
      sprintf(buf, "inf");
  } else {
    sprintf(buf, format_with_prec, x);
  }
}

#define print_op1_libm(prec, op, x, ns)                      \
  do {                                                       \
    char buf[prec + 10];                                     \
    __typeof(x) res = op(x);                                 \
    sprint_double(buf, "%." #prec "f", res, ns);             \
    printf("%s (math.h): %s\n", #op, buf);                   \
  } while (0)

#define print_op1_builtin(prec, op, x, ns)                   \
  do {                                                       \
    char buf[prec + 10];                                     \
    __typeof(x) res = __builtin_ ## op(x);                   \
    sprint_double(buf, "%." #prec "f", res, ns);             \
    printf("%s (builtin): %s\n", #op, buf);                  \
  } while (0)

#define print_op1(prec, op, x, ns)                           \
  print_op1_libm(prec, op, x, ns);                           \
  print_op1_builtin(prec, op, x, ns);

#define print_op1_llvm(prec, op, x, ns)                      \
  do {                                                       \
    char buf[prec + 10];                                     \
    __typeof(x) res = llvm_intrinsic_ ## op(x);              \
    sprint_double(buf, "%." #prec "f", res, ns);             \
    printf("%s (llvm): %s\n", #op, buf);                     \
  } while (0)

#define print_op2(prec, op, x, y, ns)                        \
  do {                                                       \
    __typeof(x) res = op(x, y);                              \
    sprint_double(buf, "%." #prec "f", res, ns);             \
    printf("%s(%f, %f) (math.h): %s\n", #op, x, y, buf);     \
    res = __builtin_ ## op(x, y);                            \
    sprint_double(buf, "%." #prec "f", res, ns);             \
    printf("%s(%f, %f) (builtin): %s\n", #op, x, y, buf);    \
  } while (0)

int main(int argc, char* argv[]) {
  int i;
  char buf[20];
  /*
   * Use no_nan_sign when the sign bit of a NaN is not consistent,
   * and just print a positive nan always in that case to get the
   * test to pass.
   */
  int no_nan_sign = 0;
  int nan_sign = 1;
  for (i = 0; i < ARRAY_SIZE_UNSAFE(f32); ++i) {
    sprint_double(buf, "%.6f", f32[i], nan_sign);
    printf("\nf32 value is: %s\n", buf);
    /*
     * We may want to fix this to have a consistent nan sign bit.
     * On x86, the llvm.sqrt intrinsic returns -nan for negative values
     * while the le32 libm/builtin always returns plain nan.
     *
     * On ARM, the llvm.sqrt intrinsic is consistent with the
     * libm/builtin function.
     *
     * However, on x86_64-nacl-clang, the builtin is the same as the intrinsic
     * and different from libm. Also, with -ffast-math, the libm function
     * call is converted to the intrinsic.
     *
     * So, the only consistent one is libm, and for the rest we conservatively
     * disable checking the sign bit.
     *
     * NOTE: change no_nan_sign to nan_sign to test.
     * https://code.google.com/p/nativeclient/issues/detail?id=4038
     */
    print_op1_libm(5, sqrtf, f32[i], no_nan_sign);
    print_op1_builtin(5, sqrtf, f32[i], no_nan_sign);
    print_op1_llvm(5, sqrtf, f32[i], no_nan_sign);
    print_op1(5, logf, f32[i], nan_sign);
    print_op1(5, log2f, f32[i], nan_sign);
    print_op1(5, log10f, f32[i], nan_sign);
    print_op1(4, expf, f32[i], nan_sign);
    print_op1(4, exp2f, f32[i], nan_sign);
    /* We may want to fix this to be consistent re: nan sign bit.
     * On x86, sin/cos of inf/-inf the functions give -nan,
     * while on ARM it is nan.
     * https://code.google.com/p/nativeclient/issues/detail?id=4040
     */
    print_op1(5, sinf, f32[i], no_nan_sign);
    print_op1(5, cosf, f32[i], no_nan_sign);
    print_op2(5, powf, neg_base32, f32[i], nan_sign);
    print_op2(5, powf, base32, f32[i], nan_sign);
  }

  for (i = 0; i < ARRAY_SIZE_UNSAFE(f64); ++i) {
    sprint_double(buf, "%.6f", f64[i], nan_sign);
    printf("\nf64 value is: %s\n", buf);
    print_op1_libm(6, sqrt, f64[i], no_nan_sign);
    print_op1_builtin(6, sqrt, f64[i], no_nan_sign);
    print_op1_llvm(6, sqrt, f64[i], no_nan_sign);
    print_op1(6, log, f64[i], nan_sign);
    print_op1(6, log2, f64[i], nan_sign);
    print_op1(6, log10, f64[i], nan_sign);
    print_op1(6, exp, f64[i], nan_sign);
    print_op1(6, exp2, f64[i], nan_sign);
    print_op1(6, sin, f64[i], no_nan_sign);
    print_op1(6, cos, f64[i], no_nan_sign);
    print_op2(6, pow, neg_base64, f64[i], nan_sign);
    print_op2(6, pow, base64, f64[i], nan_sign);
  }

  return 0;
}
