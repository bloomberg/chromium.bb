// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/TraceWrapperMember.h"

#include "core/testing/DeathAwareScriptWrappable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

using Wrapper = TraceWrapperMember<DeathAwareScriptWrappable>;

}  // namespace

TEST(TraceWrapperMemberTest, HeapVectorSwap) {
  HeapVector<Wrapper> vector1;
  DeathAwareScriptWrappable* parent1 = DeathAwareScriptWrappable::create();
  DeathAwareScriptWrappable* child1 = DeathAwareScriptWrappable::create();
  vector1.push_back(Wrapper(parent1, child1));

  HeapVector<Wrapper> vector2;
  DeathAwareScriptWrappable* parent2 = DeathAwareScriptWrappable::create();
  DeathAwareScriptWrappable* child2 = DeathAwareScriptWrappable::create();
  vector2.push_back(Wrapper(parent2, child2));

  swap(vector1, vector2, parent1, parent2);
  EXPECT_EQ(parent1, vector1.front().parent());
  EXPECT_EQ(parent2, vector2.front().parent());
}

TEST(TraceWrapperMemberTest, HeapVectorSwap2) {
  HeapVector<Wrapper> vector1;
  DeathAwareScriptWrappable* parent1 = DeathAwareScriptWrappable::create();
  DeathAwareScriptWrappable* child1 = DeathAwareScriptWrappable::create();
  vector1.push_back(Wrapper(parent1, child1));

  HeapVector<Member<DeathAwareScriptWrappable>> vector2;
  DeathAwareScriptWrappable* child2 = DeathAwareScriptWrappable::create();
  vector2.push_back(child2);

  swap(vector1, vector2, parent1);
  EXPECT_EQ(1u, vector1.size());
  EXPECT_EQ(child2, vector1.front().get());
  EXPECT_EQ(parent1, vector1.front().parent());
  EXPECT_EQ(1u, vector2.size());
  EXPECT_EQ(child1, vector2.front().get());
}

TEST(TraceWrapperMemberTest, HeapHashSet) {
  HeapHashSet<Wrapper> set;
  DeathAwareScriptWrappable* parent = DeathAwareScriptWrappable::create();

  // Loop enough so that underlying HashTable will rehash several times.
  for (int i = 1; i < 10000; ++i) {
    DeathAwareScriptWrappable* child = DeathAwareScriptWrappable::create();
    set.insert(Wrapper(parent, child));
  }
  EXPECT_EQ(9999u, set.size());

  HeapHashSet<Wrapper> set2;
  swap(set, set2);
  EXPECT_EQ(0u, set.size());
  EXPECT_EQ(9999u, set2.size());

  for (int i = 1; i < 10000; ++i) {
    auto wrapper = set2.takeAny();
    EXPECT_EQ(wrapper.parent(), parent);
  }

  EXPECT_EQ(0u, set2.size());
}

TEST(TraceWrapperMemberTest, HeapHashMapValue) {
  HeapHashMap<int, Wrapper> map;
  DeathAwareScriptWrappable* parent = DeathAwareScriptWrappable::create();

  // Loop enough so that underlying HashTable will rehash several times.
  for (int i = 1; i < 10000; ++i) {
    DeathAwareScriptWrappable* child = DeathAwareScriptWrappable::create();
    map.insert(i, Wrapper(parent, child));
  }
  EXPECT_EQ(9999u, map.size());

  for (int i = 1; i < 10000; ++i) {
    auto wrapper = map.take(i);
    EXPECT_EQ(wrapper.parent(), parent);
  }

  EXPECT_EQ(0u, map.size());
}

}  // namespace blink
