// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/wtf/RefPtr.h"

#include "platform/wtf/RefCounted.h"
#include "platform/wtf/text/StringImpl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace WTF {

TEST(RefPtrTest, Basic) {
  RefPtr<StringImpl> string;
  EXPECT_TRUE(!string);
  string = StringImpl::Create("test");
  EXPECT_TRUE(!!string);
  string.Clear();
  EXPECT_TRUE(!string);
}

TEST(RefPtrTest, LeakRef) {
  RefPtr<StringImpl> string = StringImpl::Create("test");
  EXPECT_TRUE(string);
  EXPECT_TRUE(string->HasOneRef());
  StringImpl* raw = string.Get();
  StringImpl* leaked = string.LeakRef();
  EXPECT_TRUE(!string);
  EXPECT_TRUE(leaked);
  EXPECT_TRUE(leaked->HasOneRef());
  EXPECT_EQ(raw, leaked);
  leaked->Deref();
}

TEST(RefPtrTest, MoveAssignmentOperator) {
  RefPtr<StringImpl> a = StringImpl::Create("a");
  RefPtr<StringImpl> b = StringImpl::Create("b");
  b = std::move(a);
  EXPECT_TRUE(!!b);
  EXPECT_TRUE(!a);
}

class RefCountedClass : public RefCounted<RefCountedClass> {};

TEST(RefPtrTest, ConstObject) {
  // This test is only to ensure we force the compilation of a const RefCounted
  // object to ensure the generated code compiles.
  RefPtr<const RefCountedClass> ptr_to_const = AdoptRef(new RefCountedClass());
}

}  // namespace WTF
