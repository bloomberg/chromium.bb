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

void F(int x) {}

template <typename T, void g(T)>
void H(T x) {
  g(x);
}

void Test() {
  // f should be rewritten.
  H<int, F>(0);
  // notBlinkFunction should stay the same.
  H<int, not_blink::notBlinkFunction>(1);
}

}  // namespace test_template_arg_is_function

namespace test_template_arg_is_method {

class BlinkClass {
 public:
  void Method() {}
};

template <typename T, void (T::*g)()>
void H(T&& x) {
  (x.*g)();
}

void Test() {
  // method should be rewritten.
  H<BlinkClass, &BlinkClass::Method>(BlinkClass());
  H<not_blink::NotBlinkClass, &not_blink::NotBlinkClass::notBlinkMethod>(
      not_blink::NotBlinkClass());
}

}  // namespace test_template_arg_is_method

// Test template arguments that are function templates.
template <typename T, char converter(T)>
unsigned ReallyBadHash(const T* data, unsigned length) {
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
    return ReallyBadHash<char, brokenFoldCase<char>>(data, length);
  }

 private:
  template <typename T>
  static char BrokenFoldCase(T input) {
    return input - ('a' - 'A');
  }
};

}  // namespace blink
