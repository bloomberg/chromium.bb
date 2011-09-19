/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * Test parts of math.h and floating point ops for compliance against ieee754.
 */

#include <math.h>
#include <stdio.h>
#include <errno.h>
/* #include <fenv.h>
 * TODO(jvoung) Newlib doesn't have this on any machine, except the Cell SPU.
 * Would like to test the exception flagging (see below).
 */

#define CHECK_EQ_DOUBLE(a, b)                                           \
  (a == b) ? 0 : (printf("ERROR %f != %f at %d\n", a, b, __LINE__), 1)

#define CHECK_NAN(_x_)                                                    \
  isnan(_x_) ? 0 : (printf("ERROR %s(%f) != NaN at %d\n", \
  #_x_, (double)_x_, __LINE__), 1)

#define CHECK_INF(a)                                                    \
  isinf(a) ? 0 : (printf("ERROR %f != Inf at %d\n", (double)a, __LINE__), 1)

#define ASSERT_TRUE(b)                                                  \
  (b) ? 0 : (printf("ASSERT true failed! %d at %d\n", b, __LINE__), 1)

#define ASSERT_FALSE(b)                                                 \
  (0 == (b)) ? 0 : (printf("ASSERT false failed! %d at %d\n", b, __LINE__), 1)

/************************************************************/

int test_constants() {
  int errs = 0;
  errs += CHECK_NAN(NAN);
  printf("Print out of NaN: %f\n", NAN);
  errs += CHECK_INF(INFINITY);
  printf("Print out of Infinity: %f\n", INFINITY);
  errs += CHECK_INF(1.0/0.0);
  return errs;
}

int test_compares() {
  int errs = 0;

  printf("Comparing float constants\n");
  errs += ASSERT_TRUE(NAN != NAN);
  errs += ASSERT_TRUE(isunordered(NAN, NAN));
  errs += CHECK_NAN(NAN + 3.0f);
  errs += CHECK_NAN(NAN + NAN);
  errs += CHECK_NAN(NAN - NAN);
  errs += CHECK_NAN(NAN / NAN);
  errs += CHECK_NAN(0.0 / NAN);
  errs += CHECK_NAN(0.0 * NAN);

  errs += ASSERT_FALSE(NAN == NAN);

  errs += ASSERT_TRUE(INFINITY == INFINITY);
  errs += ASSERT_FALSE(INFINITY == -INFINITY);
  errs += ASSERT_TRUE(-INFINITY == -INFINITY);
  errs += ASSERT_TRUE(INFINITY + 100.0 == INFINITY);
  errs += ASSERT_TRUE(INFINITY - 100.0 == INFINITY);
  errs += ASSERT_TRUE(-INFINITY - 100.0 == -INFINITY);
  errs += ASSERT_TRUE(-INFINITY + 100.0 == -INFINITY);
  errs += ASSERT_TRUE(-INFINITY < INFINITY);
  errs += ASSERT_FALSE(-INFINITY > INFINITY);

  errs += CHECK_NAN(0.0 * INFINITY);
  errs += CHECK_NAN(0.0 / 0.0);
  errs += CHECK_NAN(INFINITY / INFINITY);
  errs += CHECK_NAN(INFINITY - INFINITY);
  errs += CHECK_NAN(INFINITY * NAN);

  errs += CHECK_INF(INFINITY + INFINITY);
  errs += CHECK_INF(1.0 / 0.0);

  errs += ASSERT_FALSE(isfinite(INFINITY));
  errs += ASSERT_FALSE(isfinite(NAN));

  return errs;
}

/* Test non-NaN-resulting library calls. */
int test_defined() {
  int errs = 0;

  printf("Checking lib calls that take NaN, etc, but return non-NaN.\n");
  errs += CHECK_EQ_DOUBLE(pow(0.0, 0.0), 1.0);
  errs += CHECK_EQ_DOUBLE(pow(NAN, 0.0), 1.0);
  errs += CHECK_EQ_DOUBLE(pow(INFINITY, 0.0), 1.0);

  errs += CHECK_INF(sqrt(INFINITY));
  errs += CHECK_EQ_DOUBLE(sqrt(-0.0), -0.0);
  errs += CHECK_INF(pow(INFINITY, 2.0));
  errs += ASSERT_TRUE(log(0) == -INFINITY);

  return errs;
}

/* Test NaN-resulting library calls. */
int test_errs() {
  int errs = 0;

  printf("Checking well-defined library errors\n");
  errs += CHECK_NAN(pow(-3.0, 4.4));
  errs += CHECK_NAN(log(-3.0));
  errs += CHECK_NAN(sqrt(-0.001));
  errs += CHECK_NAN(asin(1.0001));
  errs += CHECK_NAN(sin(INFINITY));
  errs += CHECK_NAN(cos(INFINITY));
  errs += CHECK_NAN(acosh(0.999));
  errs += CHECK_NAN(remainder(3.3, 0.0));
  errs += CHECK_NAN(remainder(INFINITY, 3.3));
  return errs;
}

/*
  TODO(jvoung) Check if exceptions are flagged in status word
  (once fenv.h is in newlib, or we move to using glibc)?

test FE_INVALID
test FE_DIVBYZERO
test FE_INEXACT
test FE_OVERFLOW
test FE_UNDERFLOW

E.g., Ordered comparisons of NaN should raise INVALID
  (NAN < NAN);
  (NAN >= NAN);
  (1.0 >= NAN);

  as, should all the calls in test_errs()

Some notes:
http://www.kernel.org/doc/man-pages/online/pages/man7/math_error.7.html
https://www.securecoding.cert.org/confluence/display/seccode/ \
        FLP03-C.+Detect+and+handle+floating+point+errors

*/

/* Check exceptions communicated by the "old" errno mechanism */
#define CHECK_TRIPPED_ERRNO(expr)                             \
  (errno = 0,                                                 \
   (void)expr,                                                \
   (0 != errno ? 0 : (printf("%s - errno %d not set at %d\n", \
                      #expr, errno, __LINE__), 1)))

int test_exception() {
  int errs = 0;
  printf("Checking that exceptional lib calls set errno\n");
  errs += CHECK_TRIPPED_ERRNO(pow(-3, 4.4));
  errs += CHECK_TRIPPED_ERRNO(log(-3.0));
  errs += CHECK_TRIPPED_ERRNO(sqrt(-0.001));
  errs += CHECK_TRIPPED_ERRNO(asin(1.0001));

/* Some versions of libc don't set errno =(
 * http://sourceware.org/bugzilla/show_bug.cgi?id=6780
 * http://sourceware.org/bugzilla/show_bug.cgi?id=6781
  errs += CHECK_TRIPPED_ERRNO(sin(INFINITY));
  errs += CHECK_TRIPPED_ERRNO(cos(INFINITY));
  */
  errs += CHECK_TRIPPED_ERRNO(acosh(0.999));
  errs += CHECK_TRIPPED_ERRNO(remainder(3.3, 0.0));

/* Some versions of libc don't set errno =(
 * http://sourceware.org/bugzilla/show_bug.cgi?id=6783
  errs += CHECK_TRIPPED_ERRNO(remainder(INFINITY, 3.3));
  */
  return errs;
}


int main(int ac, char* av[]) {
  int errs = 0;

  errs += test_constants();
  errs += test_compares();
  errs += test_defined();
  errs += test_errs();
#if !defined(NO_ERRNO_CHECK)
  errs += test_exception();
#endif

  printf("%d errs!\n", errs);
  return errs + 55;
}
