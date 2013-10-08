// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weak_ptr_factory.h"
namespace should_succeed {

class OnlyMember {
  base::WeakPtrFactory<OnlyMember> factory_;
};

class FactoryLast {
  bool bool_member_;
  int int_member_;
  base::WeakPtrFactory<FactoryLast> factory_;
};

class FactoryRefersToOtherType {
  bool bool_member_;
  base::WeakPtrFactory<bool> bool_ptr_factory_;
};

class FirstFactoryRefersToOtherType {
  bool bool_member_;
  base::WeakPtrFactory<bool> bool_ptr_factory_;
  int int_member_;
  base::WeakPtrFactory<FirstFactoryRefersToOtherType> factory_;
};

}  // namespace should_succeed

namespace should_fail {

class FactoryFirst {
  base::WeakPtrFactory<FactoryFirst> factory_;
  int int_member;
};

class FactoryMiddle {
  bool bool_member_;
  base::WeakPtrFactory<FactoryMiddle> factory_;
  int int_member_;
};

}  // namespace should_fail

int main() {
}

