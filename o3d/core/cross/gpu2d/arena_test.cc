/*
 * Copyright 2010, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// Tests for the arena.

#include <algorithm>
#include <vector>

#include "base/scoped_ptr.h"
#include "core/cross/gpu2d/arena.h"
#include "gtest/gtest.h"

namespace o3d {
namespace gpu2d {

namespace {

// A base allocator for the Arena which tracks the regions
// which have been allocated.
class TrackedMallocAllocator : public Arena::Allocator {
 public:
  TrackedMallocAllocator() {
  }

  virtual void* Allocate(size_t size) {
    void* res = ::malloc(size);
    allocated_regions_.push_back(res);
    return res;
  }

  virtual void Free(void* ptr) {
    std::vector<void*>::iterator slot = std::find(allocated_regions_.begin(),
                                                  allocated_regions_.end(),
                                                  ptr);
    ASSERT_TRUE(slot != allocated_regions_.end());
    allocated_regions_.erase(slot);
    ::free(ptr);
  }

  bool IsEmpty() {
    return NumRegions() == 0;
  }

  int NumRegions() {
    return allocated_regions_.size();
  }

 private:
  std::vector<void*> allocated_regions_;
  DISALLOW_COPY_AND_ASSIGN(TrackedMallocAllocator);
};

// A couple of simple structs to allocate.
struct TestClass1 {
  TestClass1()
      : x(0), y(0), z(0), w(1) {
  }

  float x, y, z, w;
};

struct TestClass2 {
  TestClass2()
      : a(1), b(2), c(3), d(4) {
  }

  float a, b, c, d;
};

}  // anonymous namespace

class ArenaTest : public testing::Test {
};

// Make sure the arena can successfully allocate from more than one
// region.
TEST_F(ArenaTest, CanAllocateFromMoreThanOneRegion) {
  TrackedMallocAllocator base_allocator;
  Arena arena(&base_allocator);
  for (int i = 0; i < 10000; i++) {
    arena.Alloc<TestClass1>();
  }
  EXPECT_GT(base_allocator.NumRegions(), 1);
}

// Make sure the arena frees all allocated regions during destruction.
TEST_F(ArenaTest, FreesAllAllocatedRegions) {
  scoped_ptr<TrackedMallocAllocator> base_allocator(
      new TrackedMallocAllocator());
  {
    scoped_ptr<Arena> arena(new Arena(base_allocator.get()));
    for (int i = 0; i < 3; i++) {
      arena->Alloc<TestClass1>();
    }
    EXPECT_GT(base_allocator->NumRegions(), 0);
  }
  EXPECT_TRUE(base_allocator->IsEmpty());
}

// Make sure the arena runs constructors of the objects allocated within.
TEST_F(ArenaTest, RunsConstructors) {
  Arena arena;
  for (int i = 0; i < 10000; i++) {
    TestClass1* tc1 = arena.Alloc<TestClass1>();
    EXPECT_EQ(0, tc1->x);
    EXPECT_EQ(0, tc1->y);
    EXPECT_EQ(0, tc1->z);
    EXPECT_EQ(1, tc1->w);
    TestClass2* tc2 = arena.Alloc<TestClass2>();
    EXPECT_EQ(1, tc2->a);
    EXPECT_EQ(2, tc2->b);
    EXPECT_EQ(3, tc2->c);
    EXPECT_EQ(4, tc2->d);
  }
}

}  // namespace gpu2d
}  // namespace o3d

