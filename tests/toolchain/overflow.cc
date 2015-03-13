/*
 * Copyright 2015 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Test the overflow builtins.
 *
 * This test is run on multiple compilers and makes sure they all output the
 * same golden output values for overflow builtins. This is important to test
 * that PNaCl (which doesn't support these builtins as-is) still generates
 * correct code.
 *
 * The test performs addition/subtraction/multiplication of the minimum/maximum
 * numbers that can be represented for each datatype supported by the
 * overflow builtins.
 *
 * Documentation:
 *   clang.llvm.org/docs/LanguageExtensions.html#checked-arithmetic-builtins
 */

#include <iostream>
#include <iomanip>
#include <limits>

using namespace std;

#define NOINLINE __attribute__((noinline))

// Datatypes supported by the overflow builtins.
// DO(TYPE, SIGN, LENGTH)
#define FOR_EACH_TYPE(DO)       \
  DO(unsigned, u, )             \
  DO(unsigned long long, u, ll) \
  DO(int, s, )                  \
  DO(long long, s, ll)
static_assert(((sizeof(int) != sizeof(long)) ||
               (sizeof(long) != sizeof(long long))),
              "This test avoids testing long because it'll produce the same "
              "output as either int or long long (depending on the platform). "
              "If this weren't the case for some compilers then the golden "
              "output would be different.");

// The builtins have C-style names, create C++ overloads for them.
template <typename T> NOINLINE bool add(T, T, T*);
template <typename T> NOINLINE bool sub(T, T, T*);
template <typename T> NOINLINE bool mul(T, T, T*);

#define DECLARE_OVERLOADS(TYPE, SIGN, LENGTH)                       \
  template <>                                                       \
  NOINLINE bool add<TYPE>(TYPE lhs, TYPE rhs, TYPE * res) {         \
    return __builtin_##SIGN##add##LENGTH##_overflow(lhs, rhs, res); \
  }                                                                 \
  template <>                                                       \
  NOINLINE bool sub<TYPE>(TYPE lhs, TYPE rhs, TYPE * res) {         \
    return __builtin_##SIGN##sub##LENGTH##_overflow(lhs, rhs, res); \
  }                                                                 \
  template <>                                                       \
  NOINLINE bool mul<TYPE>(TYPE lhs, TYPE rhs, TYPE * res) {         \
    return __builtin_##SIGN##mul##LENGTH##_overflow(lhs, rhs, res); \
  }
FOR_EACH_TYPE(DECLARE_OVERLOADS)

template <typename T>
void test_single(bool (*op)(T, T, T*), char rep, T lhs, T rhs) {
  const auto w = numeric_limits<T>::digits10 + 2;
  T res;
  cout << setw(w) << lhs << ' ' << rep << ' ' << setw(w) << rhs << " = "
       << setw(w);
  if ((*op)(lhs, rhs, &res))
    cout << "overflow";
  else
    cout << res;
  cout << '\n';
}

static const int iterations = 3;

// Test the combinations of OP(LHS, RHS). We start at the *_START values and
// iterate for `iterations` repetitions using the *_OP operator on values at
// each repetition.
#define ITERATE(OP, REP, LHS_START, RHS_START, LHS_OP, RHS_OP)           \
  do {                                                                   \
    T lhs = LHS_START;                                                   \
    for (int lhs_values = 0; lhs_values != iterations; ++lhs_values) {   \
      T rhs = RHS_START;                                                 \
      for (int rhs_values = 0; rhs_values != iterations; ++rhs_values) { \
        test_single(OP<T>, REP, lhs, rhs);                               \
        RHS_OP rhs;                                                      \
      }                                                                  \
      LHS_OP lhs;                                                        \
    }                                                                    \
  } while (0)

template <typename T>
void test_type() {
  T min = numeric_limits<T>::min();
  T max = numeric_limits<T>::max();
  cout << "\nTesting " << sizeof(T) << " byte "
       << (numeric_limits<T>::is_signed ? "" : "un") << "signed integer:\n";
  ITERATE(add, '+', min, min, ++, ++);
  ITERATE(add, '+', min, max, ++, --);
  ITERATE(add, '+', max, max, --, --);
  ITERATE(add, '+', max, min, --, ++);
  ITERATE(sub, '-', min, min, ++, ++);
  ITERATE(sub, '-', min, max, ++, --);
  ITERATE(sub, '-', max, max, --, --);
  ITERATE(sub, '-', max, min, --, ++);
  ITERATE(mul, '*', min, min, ++, ++);
  ITERATE(mul, '*', min, max, ++, --);
  ITERATE(mul, '*', max, max, --, --);
  ITERATE(mul, '*', max, min, --, ++);
}

int main() {
#define TEST_TYPE(TYPE, SIGN, LENGTH) test_type<TYPE>();
  FOR_EACH_TYPE(TEST_TYPE)

  return 0;
}
