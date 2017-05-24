// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/heap/HeapCompact.h"

#include "platform/heap/Handle.h"
#include "platform/heap/SparseHeapBitmap.h"
#include "platform/wtf/Deque.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/LinkedHashSet.h"
#include "platform/wtf/Vector.h"
#include "testing/gtest/include/gtest/gtest.h"

#include <memory>

namespace {

class IntWrapper : public blink::GarbageCollectedFinalized<IntWrapper> {
 public:
  static IntWrapper* Create(int x) { return new IntWrapper(x); }

  virtual ~IntWrapper() {}

  DEFINE_INLINE_TRACE() {}

  int Value() const { return x_; }

  bool operator==(const IntWrapper& other) const {
    return other.Value() == Value();
  }

  unsigned GetHash() { return IntHash<int>::GetHash(x_); }

  IntWrapper(int x) : x_(x) {}

 private:
  IntWrapper();
  int x_;
};
static_assert(WTF::IsTraceable<IntWrapper>::value,
              "IsTraceable<> template failed to recognize trace method.");

}  // namespace

using IntVector = blink::HeapVector<blink::Member<IntWrapper>>;
using IntDeque = blink::HeapDeque<blink::Member<IntWrapper>>;
using IntMap = blink::HeapHashMap<blink::Member<IntWrapper>, int>;
// TODO(sof): decide if this ought to be a global trait specialization.
// (i.e., for HeapHash*<T>.)
WTF_ALLOW_CLEAR_UNUSED_SLOTS_WITH_MEM_FUNCTIONS(IntMap);

namespace blink {
#if ENABLE_HEAP_COMPACTION

static const size_t kChunkRange = SparseHeapBitmap::kBitmapChunkRange;
static const size_t kUnitPointer = 0x1u
                                   << SparseHeapBitmap::kPointerAlignmentInBits;

TEST(HeapCompactTest, SparseBitmapBasic) {
  Address base = reinterpret_cast<Address>(0x10000u);
  std::unique_ptr<SparseHeapBitmap> bitmap = SparseHeapBitmap::Create(base);

  size_t double_chunk = 2 * kChunkRange;

  // 101010... starting at |base|.
  for (size_t i = 0; i < double_chunk; i += 2 * kUnitPointer)
    bitmap->Add(base + i);

  // Check that hasRange() returns a bitmap subtree, if any, for a given
  // address.
  EXPECT_TRUE(!!bitmap->HasRange(base, 1));
  EXPECT_TRUE(!!bitmap->HasRange(base + kUnitPointer, 1));
  EXPECT_FALSE(!!bitmap->HasRange(base - kUnitPointer, 1));

  // Test implementation details.. that each SparseHeapBitmap node maps
  // |s_bitmapChunkRange| ranges only.
  EXPECT_EQ(bitmap->HasRange(base + kUnitPointer, 1),
            bitmap->HasRange(base + 2 * kUnitPointer, 1));
  // Second range will be just past the first.
  EXPECT_NE(bitmap->HasRange(base, 1), bitmap->HasRange(base + kChunkRange, 1));

  // Iterate a range that will encompass more than one 'chunk'.
  SparseHeapBitmap* start =
      bitmap->HasRange(base + 2 * kUnitPointer, double_chunk);
  EXPECT_TRUE(!!start);
  for (size_t i = 2 * kUnitPointer; i < double_chunk; i += 2 * kUnitPointer) {
    EXPECT_TRUE(start->IsSet(base + i));
    EXPECT_FALSE(start->IsSet(base + i + kUnitPointer));
  }
}

TEST(HeapCompactTest, SparseBitmapBuild) {
  Address base = reinterpret_cast<Address>(0x10000u);
  std::unique_ptr<SparseHeapBitmap> bitmap = SparseHeapBitmap::Create(base);

  size_t double_chunk = 2 * kChunkRange;

  // Create a sparse bitmap containing at least three chunks.
  bitmap->Add(base - double_chunk);
  bitmap->Add(base + double_chunk);

  // This is sanity testing internal implementation details of
  // SparseHeapBitmap; probing |isSet()| outside the bitmap
  // of the range used in |hasRange()|, is not defined.
  //
  // Regardless, the testing here verifies that a |hasRange()| that
  // straddles multiple internal nodes, returns a bitmap that is
  // capable of returning correct |isSet()| results.
  SparseHeapBitmap* start = bitmap->HasRange(
      base - double_chunk - 2 * kUnitPointer, 4 * kUnitPointer);
  EXPECT_TRUE(!!start);
  EXPECT_TRUE(start->IsSet(base - double_chunk));
  EXPECT_FALSE(start->IsSet(base - double_chunk + kUnitPointer));
  EXPECT_FALSE(start->IsSet(base));
  EXPECT_FALSE(start->IsSet(base + kUnitPointer));
  EXPECT_FALSE(start->IsSet(base + double_chunk));
  EXPECT_FALSE(start->IsSet(base + double_chunk + kUnitPointer));

  start = bitmap->HasRange(base - double_chunk - 2 * kUnitPointer,
                           2 * double_chunk + 2 * kUnitPointer);
  EXPECT_TRUE(!!start);
  EXPECT_TRUE(start->IsSet(base - double_chunk));
  EXPECT_FALSE(start->IsSet(base - double_chunk + kUnitPointer));
  EXPECT_TRUE(start->IsSet(base));
  EXPECT_FALSE(start->IsSet(base + kUnitPointer));
  EXPECT_TRUE(start->IsSet(base + double_chunk));
  EXPECT_FALSE(start->IsSet(base + double_chunk + kUnitPointer));

  start = bitmap->HasRange(base, 20);
  EXPECT_TRUE(!!start);
  // Probing for values outside of hasRange() should be considered
  // undefined, but do it to exercise the (left) tree traversal.
  EXPECT_TRUE(start->IsSet(base - double_chunk));
  EXPECT_FALSE(start->IsSet(base - double_chunk + kUnitPointer));
  EXPECT_TRUE(start->IsSet(base));
  EXPECT_FALSE(start->IsSet(base + kUnitPointer));
  EXPECT_TRUE(start->IsSet(base + double_chunk));
  EXPECT_FALSE(start->IsSet(base + double_chunk + kUnitPointer));

  start = bitmap->HasRange(base + kChunkRange + 2 * kUnitPointer, 2048);
  EXPECT_TRUE(!!start);
  // Probing for values outside of hasRange() should be considered
  // undefined, but do it to exercise node traversal.
  EXPECT_FALSE(start->IsSet(base - double_chunk));
  EXPECT_FALSE(start->IsSet(base - double_chunk + kUnitPointer));
  EXPECT_FALSE(start->IsSet(base));
  EXPECT_FALSE(start->IsSet(base + kUnitPointer));
  EXPECT_FALSE(start->IsSet(base + kChunkRange));
  EXPECT_TRUE(start->IsSet(base + double_chunk));
  EXPECT_FALSE(start->IsSet(base + double_chunk + kUnitPointer));
}

TEST(HeapCompactTest, SparseBitmapLeftExtension) {
  Address base = reinterpret_cast<Address>(0x10000u);
  std::unique_ptr<SparseHeapBitmap> bitmap = SparseHeapBitmap::Create(base);

  SparseHeapBitmap* start = bitmap->HasRange(base, 1);
  EXPECT_TRUE(start);

  // Verify that re-adding is a no-op.
  bitmap->Add(base);
  EXPECT_EQ(start, bitmap->HasRange(base, 1));

  // Adding an Address |A| before a single-address SparseHeapBitmap node should
  // cause that node to  be "left extended" to use |A| as its new base.
  bitmap->Add(base - 2 * kUnitPointer);
  EXPECT_EQ(bitmap->HasRange(base, 1),
            bitmap->HasRange(base - 2 * kUnitPointer, 1));

  // Reset.
  bitmap = SparseHeapBitmap::Create(base);

  // If attempting same as above, but the Address |A| is outside the
  // chunk size of a node, a new SparseHeapBitmap node needs to be
  // created to the left of |bitmap|.
  bitmap->Add(base - kChunkRange);
  EXPECT_NE(bitmap->HasRange(base, 1),
            bitmap->HasRange(base - 2 * kUnitPointer, 1));

  bitmap = SparseHeapBitmap::Create(base);
  bitmap->Add(base - kChunkRange + kUnitPointer);
  // This address is just inside the horizon and shouldn't create a new chunk.
  EXPECT_EQ(bitmap->HasRange(base, 1),
            bitmap->HasRange(base - 2 * kUnitPointer, 1));
  // ..but this one should, like for the sub-test above.
  bitmap->Add(base - kChunkRange);
  EXPECT_EQ(bitmap->HasRange(base, 1),
            bitmap->HasRange(base - 2 * kUnitPointer, 1));
  EXPECT_NE(bitmap->HasRange(base, 1), bitmap->HasRange(base - kChunkRange, 1));
}

static void PreciselyCollectGarbage() {
  ThreadState::Current()->CollectGarbage(BlinkGC::kNoHeapPointersOnStack,
                                         BlinkGC::kGCWithSweep,
                                         BlinkGC::kForcedGC);
}

static void PerformHeapCompaction() {
  EXPECT_FALSE(HeapCompact::ScheduleCompactionGCForTesting(true));
  PreciselyCollectGarbage();
  EXPECT_FALSE(HeapCompact::ScheduleCompactionGCForTesting(false));
}

// Do several GCs to make sure that later GCs don't free up old memory from
// previously run tests in this process.
static void ClearOutOldGarbage() {
  ThreadHeap& heap = ThreadState::Current()->Heap();
  while (true) {
    size_t used = heap.ObjectPayloadSizeForTesting();
    PreciselyCollectGarbage();
    if (heap.ObjectPayloadSizeForTesting() >= used)
      break;
  }
}

TEST(HeapCompactTest, CompactVector) {
  ClearOutOldGarbage();

  IntWrapper* val = IntWrapper::Create(1);
  Persistent<IntVector> vector = new IntVector(10, val);
  EXPECT_EQ(10u, vector->size());

  for (size_t i = 0; i < vector->size(); ++i)
    EXPECT_EQ(val, (*vector)[i]);

  PerformHeapCompaction();

  for (size_t i = 0; i < vector->size(); ++i)
    EXPECT_EQ(val, (*vector)[i]);
}

TEST(HeapCompactTest, CompactHashMap) {
  ClearOutOldGarbage();

  Persistent<IntMap> int_map = new IntMap();
  for (size_t i = 0; i < 100; ++i) {
    IntWrapper* val = IntWrapper::Create(i);
    int_map->insert(val, 100 - i);
  }

  EXPECT_EQ(100u, int_map->size());
  for (auto k : *int_map)
    EXPECT_EQ(k.key->Value(), 100 - k.value);

  PerformHeapCompaction();

  for (auto k : *int_map)
    EXPECT_EQ(k.key->Value(), 100 - k.value);
}

TEST(HeapCompactTest, CompactVectorPartHashMap) {
  ClearOutOldGarbage();

  using IntMapVector = HeapVector<IntMap>;

  Persistent<IntMapVector> int_map_vector = new IntMapVector();
  for (size_t i = 0; i < 10; ++i) {
    IntMap map;
    for (size_t j = 0; j < 10; ++j) {
      IntWrapper* val = IntWrapper::Create(j);
      map.insert(val, 10 - j);
    }
    int_map_vector->push_back(map);
  }

  EXPECT_EQ(10u, int_map_vector->size());
  for (auto map : *int_map_vector) {
    EXPECT_EQ(10u, map.size());
    for (auto k : map) {
      EXPECT_EQ(k.key->Value(), 10 - k.value);
    }
  }

  PerformHeapCompaction();

  EXPECT_EQ(10u, int_map_vector->size());
  for (auto map : *int_map_vector) {
    EXPECT_EQ(10u, map.size());
    for (auto k : map) {
      EXPECT_EQ(k.key->Value(), 10 - k.value);
    }
  }
}

TEST(HeapCompactTest, CompactHashPartVector) {
  ClearOutOldGarbage();

  using IntVectorMap = HeapHashMap<int, IntVector>;

  Persistent<IntVectorMap> int_vector_map = new IntVectorMap();
  for (size_t i = 0; i < 10; ++i) {
    IntVector vector;
    for (size_t j = 0; j < 10; ++j) {
      vector.push_back(IntWrapper::Create(j));
    }
    int_vector_map->insert(1 + i, vector);
  }

  EXPECT_EQ(10u, int_vector_map->size());
  for (const IntVector& int_vector : int_vector_map->Values()) {
    EXPECT_EQ(10u, int_vector.size());
    for (size_t i = 0; i < int_vector.size(); ++i) {
      EXPECT_EQ(static_cast<int>(i), int_vector[i]->Value());
    }
  }

  PerformHeapCompaction();

  EXPECT_EQ(10u, int_vector_map->size());
  for (const IntVector& int_vector : int_vector_map->Values()) {
    EXPECT_EQ(10u, int_vector.size());
    for (size_t i = 0; i < int_vector.size(); ++i) {
      EXPECT_EQ(static_cast<int>(i), int_vector[i]->Value());
    }
  }
}

TEST(HeapCompactTest, CompactDeques) {
  Persistent<IntDeque> deque = new IntDeque;
  for (int i = 0; i < 8; ++i) {
    deque->push_front(IntWrapper::Create(i));
  }
  EXPECT_EQ(8u, deque->size());

  for (size_t i = 0; i < deque->size(); ++i)
    EXPECT_EQ(static_cast<int>(7 - i), deque->at(i)->Value());

  PerformHeapCompaction();

  for (size_t i = 0; i < deque->size(); ++i)
    EXPECT_EQ(static_cast<int>(7 - i), deque->at(i)->Value());
}

TEST(HeapCompactTest, CompactDequeVectors) {
  Persistent<HeapDeque<IntVector>> deque = new HeapDeque<IntVector>;
  for (int i = 0; i < 8; ++i) {
    IntWrapper* value = IntWrapper::Create(i);
    IntVector vector = IntVector(8, value);
    deque->push_front(vector);
  }
  EXPECT_EQ(8u, deque->size());

  for (size_t i = 0; i < deque->size(); ++i)
    EXPECT_EQ(static_cast<int>(7 - i), deque->at(i).at(i)->Value());

  PerformHeapCompaction();

  for (size_t i = 0; i < deque->size(); ++i)
    EXPECT_EQ(static_cast<int>(7 - i), deque->at(i).at(i)->Value());
}

TEST(HeapCompactTest, CompactLinkedHashSet) {
  using OrderedHashSet = HeapLinkedHashSet<Member<IntWrapper>>;
  Persistent<OrderedHashSet> set = new OrderedHashSet;
  for (int i = 0; i < 13; ++i) {
    IntWrapper* value = IntWrapper::Create(i);
    set->insert(value);
  }
  EXPECT_EQ(13u, set->size());

  int expected = 0;
  for (IntWrapper* v : *set) {
    EXPECT_EQ(expected, v->Value());
    expected++;
  }

  PerformHeapCompaction();

  expected = 0;
  for (IntWrapper* v : *set) {
    EXPECT_EQ(expected, v->Value());
    expected++;
  }
}

TEST(HeapCompactTest, CompactLinkedHashSetVector) {
  using OrderedHashSet = HeapLinkedHashSet<Member<IntVector>>;
  Persistent<OrderedHashSet> set = new OrderedHashSet;
  for (int i = 0; i < 13; ++i) {
    IntWrapper* value = IntWrapper::Create(i);
    IntVector* vector = new IntVector(19, value);
    set->insert(vector);
  }
  EXPECT_EQ(13u, set->size());

  int expected = 0;
  for (IntVector* v : *set) {
    EXPECT_EQ(expected, (*v)[0]->Value());
    expected++;
  }

  PerformHeapCompaction();

  expected = 0;
  for (IntVector* v : *set) {
    EXPECT_EQ(expected, (*v)[0]->Value());
    expected++;
  }
}

TEST(HeapCompactTest, CompactLinkedHashSetMap) {
  using Inner = HeapHashSet<Member<IntWrapper>>;
  using OrderedHashSet = HeapLinkedHashSet<Member<Inner>>;

  Persistent<OrderedHashSet> set = new OrderedHashSet;
  for (int i = 0; i < 13; ++i) {
    IntWrapper* value = IntWrapper::Create(i);
    Inner* inner = new Inner;
    inner->insert(value);
    set->insert(inner);
  }
  EXPECT_EQ(13u, set->size());

  int expected = 0;
  for (const Inner* v : *set) {
    EXPECT_EQ(1u, v->size());
    EXPECT_EQ(expected, (*v->begin())->Value());
    expected++;
  }

  PerformHeapCompaction();

  expected = 0;
  for (const Inner* v : *set) {
    EXPECT_EQ(1u, v->size());
    EXPECT_EQ(expected, (*v->begin())->Value());
    expected++;
  }
}

TEST(HeapCompactTest, CompactLinkedHashSetNested) {
  using Inner = HeapLinkedHashSet<Member<IntWrapper>>;
  using OrderedHashSet = HeapLinkedHashSet<Member<Inner>>;

  Persistent<OrderedHashSet> set = new OrderedHashSet;
  for (int i = 0; i < 13; ++i) {
    IntWrapper* value = IntWrapper::Create(i);
    Inner* inner = new Inner;
    inner->insert(value);
    set->insert(inner);
  }
  EXPECT_EQ(13u, set->size());

  int expected = 0;
  for (const Inner* v : *set) {
    EXPECT_EQ(1u, v->size());
    EXPECT_EQ(expected, (*v->begin())->Value());
    expected++;
  }

  PerformHeapCompaction();

  expected = 0;
  for (const Inner* v : *set) {
    EXPECT_EQ(1u, v->size());
    EXPECT_EQ(expected, (*v->begin())->Value());
    expected++;
  }
}
#endif
}  // namespace blink
