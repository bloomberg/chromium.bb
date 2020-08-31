// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/checked_ptr.h"

class SomeClass;

class MyClass {
  MyClass() : ptr_field3_(nullptr), ptr_field7_(nullptr) {}

  // Expected rewrite: CheckedPtr<const SomeClass> ptr_field1_;
  CheckedPtr<const SomeClass> ptr_field1_;

  // Expected rewrite: CheckedPtr<volatile SomeClass> ptr_field2_;
  CheckedPtr<volatile SomeClass> ptr_field2_;

  // Expected rewrite: const CheckedPtr<SomeClass> ptr_field3_;
  const CheckedPtr<SomeClass> ptr_field3_;

  // Expected rewrite: mutable CheckedPtr<SomeClass> ptr_field4_;
  mutable CheckedPtr<SomeClass> ptr_field4_;

  // Expected rewrite: CheckedPtr<const SomeClass> ptr_field5_;
  CheckedPtr<const SomeClass> ptr_field5_;

  // Expected rewrite: volatile CheckedPtr<const SomeClass> ptr_field6_;
  volatile CheckedPtr<const SomeClass> ptr_field6_;

  // Expected rewrite: const CheckedPtr<const SomeClass> ptr_field7_;
  const CheckedPtr<const SomeClass> ptr_field7_;
};
