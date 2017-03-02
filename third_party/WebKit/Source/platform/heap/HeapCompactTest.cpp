// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/heap/HeapCompact.h"

#include "platform/heap/Handle.h"
#include "platform/heap/SparseHeapBitmap.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/Deque.h"
#include "wtf/HashMap.h"
#include "wtf/LinkedHashSet.h"
#include "wtf/Vector.h"

#include <memory>

namespace blink {
class IntWrapper;
}

using IntVector = blink::HeapVector<blink::Member<blink::IntWrapper>>;
using IntDeque = blink::HeapDeque<blink::Member<blink::IntWrapper>>;
using IntMap = blink::HeapHashMap<blink::Member<blink::IntWrapper>, int>;
// TODO(sof): decide if this ought to be a global trait specialization.
// (i.e., for HeapHash*<T>.)
WTF_ALLOW_CLEAR_UNUSED_SLOTS_WITH_MEM_FUNCTIONS(IntMap);

namespace blink {

static const size_t chunkRange = SparseHeapBitmap::s_bitmapChunkRange;
static const size_t unitPointer = 0x1u
                                  << SparseHeapBitmap::s_pointerAlignmentInBits;

TEST(HeapCompactTest, SparseBitmapBasic) {
  Address base = reinterpret_cast<Address>(0x10000u);
  std::unique_ptr<SparseHeapBitmap> bitmap = SparseHeapBitmap::create(base);

  size_t doubleChunk = 2 * chunkRange;

  // 101010... starting at |base|.
  for (size_t i = 0; i < doubleChunk; i += 2 * unitPointer)
    bitmap->add(base + i);

  // Check that hasRange() returns a bitmap subtree, if any, for a given
  // address.
  EXPECT_TRUE(!!bitmap->hasRange(base, 1));
  EXPECT_TRUE(!!bitmap->hasRange(base + unitPointer, 1));
  EXPECT_FALSE(!!bitmap->hasRange(base - unitPointer, 1));

  // Test implementation details.. that each SparseHeapBitmap node maps
  // |s_bitmapChunkRange| ranges only.
  EXPECT_EQ(bitmap->hasRange(base + unitPointer, 1),
            bitmap->hasRange(base + 2 * unitPointer, 1));
  // Second range will be just past the first.
  EXPECT_NE(bitmap->hasRange(base, 1), bitmap->hasRange(base + chunkRange, 1));

  // Iterate a range that will encompass more than one 'chunk'.
  SparseHeapBitmap* start =
      bitmap->hasRange(base + 2 * unitPointer, doubleChunk);
  EXPECT_TRUE(!!start);
  for (size_t i = 2 * unitPointer; i < doubleChunk; i += 2 * unitPointer) {
    EXPECT_TRUE(start->isSet(base + i));
    EXPECT_FALSE(start->isSet(base + i + unitPointer));
  }
}

TEST(HeapCompactTest, SparseBitmapBuild) {
  Address base = reinterpret_cast<Address>(0x10000u);
  std::unique_ptr<SparseHeapBitmap> bitmap = SparseHeapBitmap::create(base);

  size_t doubleChunk = 2 * chunkRange;

  // Create a sparse bitmap containing at least three chunks.
  bitmap->add(base - doubleChunk);
  bitmap->add(base + doubleChunk);

  // This is sanity testing internal implementation details of
  // SparseHeapBitmap; probing |isSet()| outside the bitmap
  // of the range used in |hasRange()|, is not defined.
  //
  // Regardless, the testing here verifies that a |hasRange()| that
  // straddles multiple internal nodes, returns a bitmap that is
  // capable of returning correct |isSet()| results.
  SparseHeapBitmap* start =
      bitmap->hasRange(base - doubleChunk - 2 * unitPointer, 4 * unitPointer);
  EXPECT_TRUE(!!start);
  EXPECT_TRUE(start->isSet(base - doubleChunk));
  EXPECT_FALSE(start->isSet(base - doubleChunk + unitPointer));
  EXPECT_FALSE(start->isSet(base));
  EXPECT_FALSE(start->isSet(base + unitPointer));
  EXPECT_FALSE(start->isSet(base + doubleChunk));
  EXPECT_FALSE(start->isSet(base + doubleChunk + unitPointer));

  start = bitmap->hasRange(base - doubleChunk - 2 * unitPointer,
                           2 * doubleChunk + 2 * unitPointer);
  EXPECT_TRUE(!!start);
  EXPECT_TRUE(start->isSet(base - doubleChunk));
  EXPECT_FALSE(start->isSet(base - doubleChunk + unitPointer));
  EXPECT_TRUE(start->isSet(base));
  EXPECT_FALSE(start->isSet(base + unitPointer));
  EXPECT_TRUE(start->isSet(base + doubleChunk));
  EXPECT_FALSE(start->isSet(base + doubleChunk + unitPointer));

  start = bitmap->hasRange(base, 20);
  EXPECT_TRUE(!!start);
  // Probing for values outside of hasRange() should be considered
  // undefined, but do it to exercise the (left) tree traversal.
  EXPECT_TRUE(start->isSet(base - doubleChunk));
  EXPECT_FALSE(start->isSet(base - doubleChunk + unitPointer));
  EXPECT_TRUE(start->isSet(base));
  EXPECT_FALSE(start->isSet(base + unitPointer));
  EXPECT_TRUE(start->isSet(base + doubleChunk));
  EXPECT_FALSE(start->isSet(base + doubleChunk + unitPointer));

  start = bitmap->hasRange(base + chunkRange + 2 * unitPointer, 2048);
  EXPECT_TRUE(!!start);
  // Probing for values outside of hasRange() should be considered
  // undefined, but do it to exercise node traversal.
  EXPECT_FALSE(start->isSet(base - doubleChunk));
  EXPECT_FALSE(start->isSet(base - doubleChunk + unitPointer));
  EXPECT_FALSE(start->isSet(base));
  EXPECT_FALSE(start->isSet(base + unitPointer));
  EXPECT_FALSE(start->isSet(base + chunkRange));
  EXPECT_TRUE(start->isSet(base + doubleChunk));
  EXPECT_FALSE(start->isSet(base + doubleChunk + unitPointer));
}

TEST(HeapCompactTest, SparseBitmapLeftExtension) {
  Address base = reinterpret_cast<Address>(0x10000u);
  std::unique_ptr<SparseHeapBitmap> bitmap = SparseHeapBitmap::create(base);

  SparseHeapBitmap* start = bitmap->hasRange(base, 1);
  EXPECT_TRUE(start);

  // Verify that re-adding is a no-op.
  bitmap->add(base);
  EXPECT_EQ(start, bitmap->hasRange(base, 1));

  // Adding an Address |A| before a single-address SparseHeapBitmap node should
  // cause that node to  be "left extended" to use |A| as its new base.
  bitmap->add(base - 2 * unitPointer);
  EXPECT_EQ(bitmap->hasRange(base, 1),
            bitmap->hasRange(base - 2 * unitPointer, 1));

  // Reset.
  bitmap = SparseHeapBitmap::create(base);

  // If attempting same as above, but the Address |A| is outside the
  // chunk size of a node, a new SparseHeapBitmap node needs to be
  // created to the left of |bitmap|.
  bitmap->add(base - chunkRange);
  EXPECT_NE(bitmap->hasRange(base, 1),
            bitmap->hasRange(base - 2 * unitPointer, 1));

  bitmap = SparseHeapBitmap::create(base);
  bitmap->add(base - chunkRange + unitPointer);
  // This address is just inside the horizon and shouldn't create a new chunk.
  EXPECT_EQ(bitmap->hasRange(base, 1),
            bitmap->hasRange(base - 2 * unitPointer, 1));
  // ..but this one should, like for the sub-test above.
  bitmap->add(base - chunkRange);
  EXPECT_EQ(bitmap->hasRange(base, 1),
            bitmap->hasRange(base - 2 * unitPointer, 1));
  EXPECT_NE(bitmap->hasRange(base, 1), bitmap->hasRange(base - chunkRange, 1));
}

static void preciselyCollectGarbage() {
  ThreadState::current()->collectGarbage(
      BlinkGC::NoHeapPointersOnStack, BlinkGC::GCWithSweep, BlinkGC::ForcedGC);
}

static void performHeapCompaction() {
  EXPECT_FALSE(HeapCompact::scheduleCompactionGCForTesting(true));
  preciselyCollectGarbage();
  EXPECT_FALSE(HeapCompact::scheduleCompactionGCForTesting(false));
}

// Do several GCs to make sure that later GCs don't free up old memory from
// previously run tests in this process.
static void clearOutOldGarbage() {
  ThreadHeap& heap = ThreadState::current()->heap();
  while (true) {
    size_t used = heap.objectPayloadSizeForTesting();
    preciselyCollectGarbage();
    if (heap.objectPayloadSizeForTesting() >= used)
      break;
  }
}

class IntWrapper : public GarbageCollectedFinalized<IntWrapper> {
 public:
  static IntWrapper* create(int x) { return new IntWrapper(x); }

  virtual ~IntWrapper() { ++s_destructorCalls; }

  static int s_destructorCalls;
  DEFINE_INLINE_TRACE() {}

  int value() const { return m_x; }

  bool operator==(const IntWrapper& other) const {
    return other.value() == value();
  }

  unsigned hash() { return IntHash<int>::hash(m_x); }

  IntWrapper(int x) : m_x(x) {}

 private:
  IntWrapper();
  int m_x;
};
static_assert(WTF::IsTraceable<IntWrapper>::value,
              "IsTraceable<> template failed to recognize trace method.");

TEST(HeapCompactTest, CompactVector) {
  clearOutOldGarbage();

  IntWrapper* val = IntWrapper::create(1);
  Persistent<IntVector> vector = new IntVector(10, val);
  EXPECT_EQ(10u, vector->size());

  for (size_t i = 0; i < vector->size(); ++i)
    EXPECT_EQ(val, (*vector)[i]);

  performHeapCompaction();

  for (size_t i = 0; i < vector->size(); ++i)
    EXPECT_EQ(val, (*vector)[i]);
}

TEST(HeapCompactTest, CompactHashMap) {
  clearOutOldGarbage();

  Persistent<IntMap> intMap = new IntMap();
  for (size_t i = 0; i < 100; ++i) {
    IntWrapper* val = IntWrapper::create(i);
    intMap->insert(val, 100 - i);
  }

  EXPECT_EQ(100u, intMap->size());
  for (auto k : *intMap)
    EXPECT_EQ(k.key->value(), 100 - k.value);

  performHeapCompaction();

  for (auto k : *intMap)
    EXPECT_EQ(k.key->value(), 100 - k.value);
}

TEST(HeapCompactTest, CompactVectorPartHashMap) {
  clearOutOldGarbage();

  using IntMapVector = HeapVector<IntMap>;

  Persistent<IntMapVector> intMapVector = new IntMapVector();
  for (size_t i = 0; i < 10; ++i) {
    IntMap map;
    for (size_t j = 0; j < 10; ++j) {
      IntWrapper* val = IntWrapper::create(j);
      map.insert(val, 10 - j);
    }
    intMapVector->push_back(map);
  }

  EXPECT_EQ(10u, intMapVector->size());
  for (auto map : *intMapVector) {
    EXPECT_EQ(10u, map.size());
    for (auto k : map) {
      EXPECT_EQ(k.key->value(), 10 - k.value);
    }
  }

  performHeapCompaction();

  EXPECT_EQ(10u, intMapVector->size());
  for (auto map : *intMapVector) {
    EXPECT_EQ(10u, map.size());
    for (auto k : map) {
      EXPECT_EQ(k.key->value(), 10 - k.value);
    }
  }
}

TEST(HeapCompactTest, CompactHashPartVector) {
  clearOutOldGarbage();

  using IntVectorMap = HeapHashMap<int, IntVector>;

  Persistent<IntVectorMap> intVectorMap = new IntVectorMap();
  for (size_t i = 0; i < 10; ++i) {
    IntVector vector;
    for (size_t j = 0; j < 10; ++j) {
      vector.push_back(IntWrapper::create(j));
    }
    intVectorMap->insert(1 + i, vector);
  }

  EXPECT_EQ(10u, intVectorMap->size());
  for (const IntVector& intVector : intVectorMap->values()) {
    EXPECT_EQ(10u, intVector.size());
    for (size_t i = 0; i < intVector.size(); ++i) {
      EXPECT_EQ(static_cast<int>(i), intVector[i]->value());
    }
  }

  performHeapCompaction();

  EXPECT_EQ(10u, intVectorMap->size());
  for (const IntVector& intVector : intVectorMap->values()) {
    EXPECT_EQ(10u, intVector.size());
    for (size_t i = 0; i < intVector.size(); ++i) {
      EXPECT_EQ(static_cast<int>(i), intVector[i]->value());
    }
  }
}

TEST(HeapCompactTest, CompactDeques) {
  Persistent<IntDeque> deque = new IntDeque;
  for (int i = 0; i < 8; ++i) {
    deque->prepend(IntWrapper::create(i));
  }
  EXPECT_EQ(8u, deque->size());

  for (size_t i = 0; i < deque->size(); ++i)
    EXPECT_EQ(static_cast<int>(7 - i), deque->at(i)->value());

  performHeapCompaction();

  for (size_t i = 0; i < deque->size(); ++i)
    EXPECT_EQ(static_cast<int>(7 - i), deque->at(i)->value());
}

TEST(HeapCompactTest, CompactDequeVectors) {
  Persistent<HeapDeque<IntVector>> deque = new HeapDeque<IntVector>;
  for (int i = 0; i < 8; ++i) {
    IntWrapper* value = IntWrapper::create(i);
    IntVector vector = IntVector(8, value);
    deque->prepend(vector);
  }
  EXPECT_EQ(8u, deque->size());

  for (size_t i = 0; i < deque->size(); ++i)
    EXPECT_EQ(static_cast<int>(7 - i), deque->at(i).at(i)->value());

  performHeapCompaction();

  for (size_t i = 0; i < deque->size(); ++i)
    EXPECT_EQ(static_cast<int>(7 - i), deque->at(i).at(i)->value());
}

TEST(HeapCompactTest, CompactLinkedHashSet) {
  using OrderedHashSet = HeapLinkedHashSet<Member<IntWrapper>>;
  Persistent<OrderedHashSet> set = new OrderedHashSet;
  for (int i = 0; i < 13; ++i) {
    IntWrapper* value = IntWrapper::create(i);
    set->insert(value);
  }
  EXPECT_EQ(13u, set->size());

  int expected = 0;
  for (IntWrapper* v : *set) {
    EXPECT_EQ(expected, v->value());
    expected++;
  }

  performHeapCompaction();

  expected = 0;
  for (IntWrapper* v : *set) {
    EXPECT_EQ(expected, v->value());
    expected++;
  }
}

TEST(HeapCompactTest, CompactLinkedHashSetVector) {
  using OrderedHashSet = HeapLinkedHashSet<Member<IntVector>>;
  Persistent<OrderedHashSet> set = new OrderedHashSet;
  for (int i = 0; i < 13; ++i) {
    IntWrapper* value = IntWrapper::create(i);
    IntVector* vector = new IntVector(19, value);
    set->insert(vector);
  }
  EXPECT_EQ(13u, set->size());

  int expected = 0;
  for (IntVector* v : *set) {
    EXPECT_EQ(expected, (*v)[0]->value());
    expected++;
  }

  performHeapCompaction();

  expected = 0;
  for (IntVector* v : *set) {
    EXPECT_EQ(expected, (*v)[0]->value());
    expected++;
  }
}

TEST(HeapCompactTest, CompactLinkedHashSetMap) {
  using Inner = HeapHashSet<Member<IntWrapper>>;
  using OrderedHashSet = HeapLinkedHashSet<Member<Inner>>;

  Persistent<OrderedHashSet> set = new OrderedHashSet;
  for (int i = 0; i < 13; ++i) {
    IntWrapper* value = IntWrapper::create(i);
    Inner* inner = new Inner;
    inner->insert(value);
    set->insert(inner);
  }
  EXPECT_EQ(13u, set->size());

  int expected = 0;
  for (const Inner* v : *set) {
    EXPECT_EQ(1u, v->size());
    EXPECT_EQ(expected, (*v->begin())->value());
    expected++;
  }

  performHeapCompaction();

  expected = 0;
  for (const Inner* v : *set) {
    EXPECT_EQ(1u, v->size());
    EXPECT_EQ(expected, (*v->begin())->value());
    expected++;
  }
}

TEST(HeapCompactTest, CompactLinkedHashSetNested) {
  using Inner = HeapLinkedHashSet<Member<IntWrapper>>;
  using OrderedHashSet = HeapLinkedHashSet<Member<Inner>>;

  Persistent<OrderedHashSet> set = new OrderedHashSet;
  for (int i = 0; i < 13; ++i) {
    IntWrapper* value = IntWrapper::create(i);
    Inner* inner = new Inner;
    inner->insert(value);
    set->insert(inner);
  }
  EXPECT_EQ(13u, set->size());

  int expected = 0;
  for (const Inner* v : *set) {
    EXPECT_EQ(1u, v->size());
    EXPECT_EQ(expected, (*v->begin())->value());
    expected++;
  }

  performHeapCompaction();

  expected = 0;
  for (const Inner* v : *set) {
    EXPECT_EQ(1u, v->size());
    EXPECT_EQ(expected, (*v->begin())->value());
    expected++;
  }
}

}  // namespace blink
