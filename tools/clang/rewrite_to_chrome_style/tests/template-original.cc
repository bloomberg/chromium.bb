// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

namespace not_blink {

void notBlinkFunction(int x) {}

class NotBlinkClass {
 public:
  void notBlinkMethod() {}
};

template <typename T>
void notBlinkFunctionTemplate(T x) {}

}  // not_blink

namespace blink {

template <typename T, int number>
void F() {
  // We don't assert on this, and we don't end up considering it a const for
  // now.
  const int maybe_a_const = sizeof(T);
  const int is_a_const = number;
}

template <int number, typename... T>
void F() {
  // We don't assert on this, and we don't end up considering it a const for
  // now.
  const int maybe_a_const = sizeof...(T);
  const int is_a_const = number;
}

namespace test_template_arg_is_function {

void f(int x) {}

template <typename T, void g(T)>
void h(T x) {
  g(x);
}

void test() {
  // f should be rewritten.
  h<int, f>(0);
  // notBlinkFunction should stay the same.
  h<int, not_blink::notBlinkFunction>(1);
}

}  // namespace test_template_arg_is_function

namespace test_template_arg_is_method {

class BlinkClass {
 public:
  void method() {}
};

template <typename T, void (T::*g)()>
void h(T&& x) {
  (x.*g)();
}

void test() {
  // method should be rewritten.
  h<BlinkClass, &BlinkClass::method>(BlinkClass());
  h<not_blink::NotBlinkClass, &not_blink::NotBlinkClass::notBlinkMethod>(
      not_blink::NotBlinkClass());
}

}  // namespace test_template_arg_is_method

// Test template arguments that are function templates.
template <typename T, char converter(T)>
unsigned reallyBadHash(const T* data, unsigned length) {
  unsigned hash = 1;
  for (unsigned i = 0; i < length; ++i) {
    hash *= converter(data[i]);
  }
  return hash;
}

struct StringHasher {
  static unsigned Hash(const char* data, unsigned length) {
    // TODO(dcheng): For some reason, brokenFoldCase gets parsed as an
    // UnresolvedLookupExpr, so this doesn't get rewritten. Meh. See
    // https://crbug.com/584408.
    return reallyBadHash<char, brokenFoldCase<char>>(data, length);
  }

 private:
  template <typename T>
  static char brokenFoldCase(T input) {
    return input - ('a' - 'A');
  }
};

}  // namespace blink
