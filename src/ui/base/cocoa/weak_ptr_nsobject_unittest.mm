// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/base/cocoa/weak_ptr_nsobject.h"

#import "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ui {
namespace {

class ScopedFoo {
 public:
  ScopedFoo() : factory_(this) {}

  WeakPtrNSObject* GetWeakPtr() { return factory_.handle(); }

 private:
  WeakPtrNSObjectFactory<ScopedFoo> factory_;

  DISALLOW_COPY_AND_ASSIGN(ScopedFoo);
};

}  // namespace

// Ensure WeakPtrNSObjectFactory invalidates weak pointers correctly.
TEST(WeakPtrNSObjectTest, EnsureInvalidated) {
  base::scoped_nsobject<WeakPtrNSObject> weak_ptr;
  {
    ScopedFoo foo;
    weak_ptr.reset([foo.GetWeakPtr() retain]);
    EXPECT_EQ(&foo, WeakPtrNSObjectFactory<ScopedFoo>::Get(weak_ptr));
  }
  EXPECT_EQ(nullptr, WeakPtrNSObjectFactory<ScopedFoo>::Get(weak_ptr));
}

}  // namespace ui
