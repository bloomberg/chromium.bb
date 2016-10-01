// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wtf/MakeCancellable.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/Compiler.h"

namespace WTF {
namespace {

void add(int* x, int y) {
  *x += y;
}

class DestructionCounter {
 public:
  explicit DestructionCounter(int* counter) : m_counter(counter) {}
  ~DestructionCounter() {
    if (m_counter)
      ++*m_counter;
  }

  DestructionCounter(DestructionCounter&& other) : m_counter(other.m_counter) {
    other.m_counter = nullptr;
  }

 private:
  int* m_counter;
};

class RefCountedDestructionCounter
    : public RefCounted<RefCountedDestructionCounter>,
      public DestructionCounter {
 public:
  explicit RefCountedDestructionCounter(int* counter)
      : DestructionCounter(counter) {}
};

}  // namespace

TEST(MakeCancellableTest, NotCancelled) {
  int v = 0;
  auto f = bind(&add, unretained(&v));
  auto result = makeCancellable(std::move(f));

  EXPECT_EQ(0, v);
  (*result.function)(3);
  EXPECT_EQ(3, v);
}

TEST(MakeCancellableTest, ExplicitCancel) {
  int v = 0;
  auto f = bind(&add, unretained(&v));
  auto result = makeCancellable(std::move(f));

  EXPECT_TRUE(result.canceller.isActive());
  result.canceller.cancel();
  EXPECT_FALSE(result.canceller.isActive());

  EXPECT_EQ(0, v);
  (*result.function)(3);
  EXPECT_EQ(0, v);
}

TEST(MakeCancellableTest, ScopeOutCancel) {
  int v = 0;
  auto f = bind(&add, unretained(&v));
  {
    auto result = makeCancellable(std::move(f));
    f = std::move(result.function);

    ScopedFunctionCanceller scopedCanceller = std::move(result.canceller);
    EXPECT_TRUE(scopedCanceller.isActive());
  }

  EXPECT_EQ(0, v);
  (*f)(3);
  EXPECT_EQ(0, v);
}

TEST(MakeCancellableTest, Detach) {
  int v = 0;
  auto f = bind(&add, unretained(&v));
  {
    auto result = makeCancellable(std::move(f));
    f = std::move(result.function);

    ScopedFunctionCanceller scopedCanceller = std::move(result.canceller);
    EXPECT_TRUE(scopedCanceller.isActive());
    scopedCanceller.detach();
    EXPECT_FALSE(scopedCanceller.isActive());
  }

  EXPECT_EQ(0, v);
  (*f)(3);
  EXPECT_EQ(3, v);
}

TEST(MakeCancellableTest, MultiCall) {
  int v = 0;
  auto f = bind(&add, unretained(&v));
  auto result = makeCancellable(std::move(f));

  EXPECT_EQ(0, v);
  (*result.function)(2);
  EXPECT_EQ(2, v);
  (*result.function)(3);
  EXPECT_EQ(5, v);
}

TEST(MakeCancellableTest, DestroyOnCancel) {
  int counter = 0;
  auto f = bind([](const DestructionCounter&) {}, DestructionCounter(&counter));
  auto result = makeCancellable(std::move(f));

  EXPECT_EQ(0, counter);
  result.canceller.cancel();
  EXPECT_EQ(1, counter);
}

TEST(MakeCancellableTest, DestroyOnWrapperDestruction) {
  int counter = 0;
  auto f = bind([](const DestructionCounter&) {}, DestructionCounter(&counter));
  auto result = makeCancellable(std::move(f));

  EXPECT_TRUE(result.canceller.isActive());

  EXPECT_EQ(0, counter);
  result.function = nullptr;
  EXPECT_EQ(1, counter);

  EXPECT_FALSE(result.canceller.isActive());
}

TEST(MakeCancellableTest, SelfAssignment) {
  int v = 0;
  auto f = bind(&add, unretained(&v));
  auto result = makeCancellable(std::move(f));

  ScopedFunctionCanceller scopedCanceller = std::move(result.canceller);
  EXPECT_TRUE(scopedCanceller.isActive());

#if COMPILER(CLANG)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wself-move"
  scopedCanceller = std::move(scopedCanceller);
#pragma clang diagnostic pop
#else
  scopedCanceller = std::move(scopedCanceller);
#endif

  EXPECT_TRUE(scopedCanceller.isActive());
  EXPECT_EQ(0, v);
  (*result.function)(1);
  EXPECT_EQ(1, v);
}

}  // namespace WTF
