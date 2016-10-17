// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/base/intrusive_heap.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace scheduler {
namespace {

struct TestElement {
  int key;
  HeapHandle* handle;

  bool operator<=(const TestElement& other) const { return key <= other.key; }

  void SetHeapHandle(HeapHandle h) {
    if (handle)
      *handle = h;
  }
};

}  // namespace

using IntrusiveHeapTest = ::testing::Test;

TEST(IntrusiveHeapTest, Basic) {
  IntrusiveHeap<TestElement> heap;

  EXPECT_TRUE(heap.empty());
  EXPECT_EQ(0u, heap.size());
}

TEST(IntrusiveHeapTest, Min) {
  IntrusiveHeap<TestElement> heap;

  heap.insert({9, nullptr});
  heap.insert({10, nullptr});
  heap.insert({8, nullptr});
  heap.insert({2, nullptr});
  heap.insert({7, nullptr});
  heap.insert({15, nullptr});
  heap.insert({22, nullptr});
  heap.insert({3, nullptr});

  EXPECT_FALSE(heap.empty());
  EXPECT_EQ(8u, heap.size());
  EXPECT_EQ(2, heap.min().key);
}

TEST(IntrusiveHeapTest, InsertAscending) {
  IntrusiveHeap<TestElement> heap;

  for (int i = 0; i < 50; i++)
    heap.insert({i, nullptr});

  EXPECT_EQ(0, heap.min().key);
  EXPECT_EQ(50u, heap.size());
}

TEST(IntrusiveHeapTest, InsertDescending) {
  IntrusiveHeap<TestElement> heap;

  for (int i = 0; i < 50; i++)
    heap.insert({50 - i, nullptr});

  EXPECT_EQ(1, heap.min().key);
  EXPECT_EQ(50u, heap.size());
}

TEST(IntrusiveHeapTest, HeapIndex) {
  IntrusiveHeap<TestElement> heap;
  HeapHandle index5;
  HeapHandle index4;
  HeapHandle index3;
  HeapHandle index2;
  HeapHandle index1;

  EXPECT_FALSE(index1.IsValid());
  EXPECT_FALSE(index2.IsValid());
  EXPECT_FALSE(index3.IsValid());
  EXPECT_FALSE(index4.IsValid());
  EXPECT_FALSE(index5.IsValid());

  heap.insert({15, &index5});
  heap.insert({14, &index4});
  heap.insert({13, &index3});
  heap.insert({12, &index2});
  heap.insert({11, &index1});

  EXPECT_TRUE(index1.IsValid());
  EXPECT_TRUE(index2.IsValid());
  EXPECT_TRUE(index3.IsValid());
  EXPECT_TRUE(index4.IsValid());
  EXPECT_TRUE(index5.IsValid());

  EXPECT_FALSE(heap.empty());
}

TEST(IntrusiveHeapTest, Pop) {
  IntrusiveHeap<TestElement> heap;

  for (int i = 0; i < 500; i++)
    heap.insert({i, nullptr});

  EXPECT_FALSE(heap.empty());
  EXPECT_EQ(500u, heap.size());
  for (int i = 0; i < 500; i++) {
    EXPECT_EQ(i, heap.min().key);
    heap.pop();
  }
  EXPECT_TRUE(heap.empty());
}

TEST(IntrusiveHeapTest, Erase) {
  IntrusiveHeap<TestElement> heap;

  HeapHandle index12;

  heap.insert({15, nullptr});
  heap.insert({14, nullptr});
  heap.insert({13, nullptr});
  heap.insert({12, &index12});
  heap.insert({11, nullptr});

  EXPECT_EQ(5u, heap.size());
  heap.erase(index12);
  EXPECT_EQ(4u, heap.size());

  EXPECT_EQ(11, heap.min().key);
  heap.pop();
  EXPECT_EQ(13, heap.min().key);
  heap.pop();
  EXPECT_EQ(14, heap.min().key);
  heap.pop();
  EXPECT_EQ(15, heap.min().key);
  heap.pop();
  EXPECT_TRUE(heap.empty());
}

TEST(IntrusiveHeapTest, ReplaceMin) {
  IntrusiveHeap<TestElement> heap;

  for (int i = 0; i < 500; i++)
    heap.insert({500 - i, nullptr});

  EXPECT_EQ(1, heap.min().key);

  for (int i = 0; i < 500; i++)
    heap.ReplaceMin({1000 + i, nullptr});

  EXPECT_EQ(1000, heap.min().key);
}

TEST(IntrusiveHeapTest, ReplaceMinWithNonLeafNode) {
  IntrusiveHeap<TestElement> heap;

  for (int i = 0; i < 50; i++) {
    heap.insert({i, nullptr});
    heap.insert({200 + i, nullptr});
  }

  EXPECT_EQ(0, heap.min().key);

  for (int i = 0; i < 50; i++)
    heap.ReplaceMin({100 + i, nullptr});

  for (int i = 0; i < 50; i++) {
    EXPECT_EQ((100 + i), heap.min().key);
    heap.pop();
  }
  for (int i = 0; i < 50; i++) {
    EXPECT_EQ((200 + i), heap.min().key);
    heap.pop();
  }
  EXPECT_TRUE(heap.empty());
}

}  // namespace scheduler
}  // namespace blink
