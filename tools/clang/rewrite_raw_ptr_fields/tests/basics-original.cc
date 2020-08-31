// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class SomeClass;

class MyClass {
  // Expected rewrite: CheckedPtr<SomeClass> raw_ptr_field;
  SomeClass* raw_ptr_field;

  // No rewrite expected.
  int int_field;
};

struct MyStruct {
  // Expected rewrite: CheckedPtr<SomeClass> raw_ptr_field;
  SomeClass* raw_ptr_field;

  // No rewrite expected.
  int int_field;
};

template <typename T>
class MyTemplate {
  // Expected rewrite: CheckedPtr<T> raw_ptr_field;
  T* raw_ptr_field;

  // No rewrite expected.
  int int_field;
};

// The field below won't compile without the |typename| keyword (because
// at this point we don't know if MaybeProvidesType<T>::Type is a type,
// value or something else).  Let's see if the rewriter will preserve
// preserve the |typename| keyword.
template <typename T>
struct MaybeProvidesType;
template <typename T>
struct DependentNameTest {
  // Expected rewrite: CheckedPtr<typename MaybeProvidesType<T>::Type> field;
  typename MaybeProvidesType<T>::Type* field;
};
