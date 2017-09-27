// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/wtf/RefPtr.h"

#include "platform/wtf/RefCounted.h"
#include "platform/wtf/ThreadSafeRefCounted.h"
#include "platform/wtf/text/StringImpl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace WTF {
namespace {

TEST(RefPtrTest, Basic) {
  RefPtr<StringImpl> string;
  EXPECT_TRUE(!string);
  string = StringImpl::Create("test");
  EXPECT_TRUE(!!string);
  string = nullptr;
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
  RefPtr<const RefCountedClass> ptr_to_const =
      WTF::AdoptRef(new RefCountedClass());
}

class CustomDeleter;

struct Deleter {
  static void Destruct(const CustomDeleter*);
};

class CustomDeleter : public RefCounted<CustomDeleter, Deleter> {
 public:
  explicit CustomDeleter(bool* deleted) : deleted_(deleted) {}

 private:
  friend struct Deleter;
  ~CustomDeleter() {}

  bool* deleted_;
};

void Deleter::Destruct(const CustomDeleter* obj) {
  EXPECT_FALSE(*obj->deleted_);
  *obj->deleted_ = true;
  delete obj;
}

TEST(RefPtrTest, CustomDeleter) {
  bool deleted = false;
  RefPtr<CustomDeleter> obj = WTF::AdoptRef(new CustomDeleter(&deleted));
  EXPECT_FALSE(deleted);
  obj = nullptr;
  EXPECT_TRUE(deleted);
}

class CustomDeleterThreadSafe;

struct DeleterThreadSafe {
  static void Destruct(const CustomDeleterThreadSafe*);
};

class CustomDeleterThreadSafe
    : public ThreadSafeRefCounted<CustomDeleterThreadSafe, DeleterThreadSafe> {
 public:
  explicit CustomDeleterThreadSafe(bool* deleted) : deleted_(deleted) {}

 private:
  friend struct DeleterThreadSafe;
  ~CustomDeleterThreadSafe() {}

  bool* deleted_;
};

void DeleterThreadSafe::Destruct(const CustomDeleterThreadSafe* obj) {
  EXPECT_FALSE(*obj->deleted_);
  *obj->deleted_ = true;
  delete obj;
}

TEST(RefPtrTest, CustomDeleterThreadSafe) {
  bool deleted = false;
  RefPtr<CustomDeleterThreadSafe> obj =
      WTF::AdoptRef(new CustomDeleterThreadSafe(&deleted));
  EXPECT_FALSE(deleted);
  obj = nullptr;
  EXPECT_TRUE(deleted);
}

}  // namespace
}  // namespace WTF
