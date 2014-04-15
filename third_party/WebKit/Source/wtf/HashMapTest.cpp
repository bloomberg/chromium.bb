/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "wtf/HashMap.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include <gtest/gtest.h>

namespace {

typedef WTF::HashMap<int, int> IntHashMap;

TEST(WTF, HashTableIteratorComparison)
{
    IntHashMap map;
    map.add(1, 2);
    ASSERT_TRUE(map.begin() != map.end());
    ASSERT_FALSE(map.begin() == map.end());

    IntHashMap::const_iterator begin = map.begin();
    ASSERT_TRUE(begin == map.begin());
    ASSERT_TRUE(map.begin() == begin);
    ASSERT_TRUE(begin != map.end());
    ASSERT_TRUE(map.end() != begin);
    ASSERT_FALSE(begin != map.begin());
    ASSERT_FALSE(map.begin() != begin);
    ASSERT_FALSE(begin == map.end());
    ASSERT_FALSE(map.end() == begin);
}

struct TestDoubleHashTraits : HashTraits<double> {
    static const unsigned minimumTableSize = 8;
};

typedef HashMap<double, int64_t, DefaultHash<double>::Hash, TestDoubleHashTraits> DoubleHashMap;

static int bucketForKey(double key)
{
    return DefaultHash<double>::Hash::hash(key) & (TestDoubleHashTraits::minimumTableSize - 1);
}

TEST(WTF, DoubleHashCollisions)
{
    // The "clobber" key here is one that ends up stealing the bucket that the -0 key
    // originally wants to be in. This makes the 0 and -0 keys collide and the test then
    // fails unless the FloatHash::equals() implementation can distinguish them.
    const double clobberKey = 6;
    const double zeroKey = 0;
    const double negativeZeroKey = -zeroKey;

    DoubleHashMap map;

    map.add(clobberKey, 1);
    map.add(zeroKey, 2);
    map.add(negativeZeroKey, 3);

    ASSERT_EQ(bucketForKey(clobberKey), bucketForKey(negativeZeroKey));
    ASSERT_EQ(map.get(clobberKey), 1);
    ASSERT_EQ(map.get(zeroKey), 2);
    ASSERT_EQ(map.get(negativeZeroKey), 3);
}

class DestructCounter {
public:
    explicit DestructCounter(int i, int* destructNumber)
        : m_i(i)
        , m_destructNumber(destructNumber)
    { }

    ~DestructCounter() { ++(*m_destructNumber); }
    int get() const { return m_i; }

private:
    int m_i;
    int* m_destructNumber;
};

typedef WTF::HashMap<int, OwnPtr<DestructCounter> > OwnPtrHashMap;

TEST(WTF, HashMapWithOwnPtrAsValue)
{
    int destructNumber = 0;
    OwnPtrHashMap map;
    map.add(1, adoptPtr(new DestructCounter(1, &destructNumber)));
    map.add(2, adoptPtr(new DestructCounter(2, &destructNumber)));

    DestructCounter* counter1 = map.get(1);
    ASSERT_EQ(1, counter1->get());
    DestructCounter* counter2 = map.get(2);
    ASSERT_EQ(2, counter2->get());
    ASSERT_EQ(0, destructNumber);

    for (OwnPtrHashMap::iterator iter = map.begin(); iter != map.end(); ++iter) {
        OwnPtr<DestructCounter>& ownCounter = iter->value;
        ASSERT_EQ(iter->key, ownCounter->get());
    }
    ASSERT_EQ(0, destructNumber);

    OwnPtr<DestructCounter> ownCounter1 = map.take(1);
    ASSERT_EQ(ownCounter1.get(), counter1);
    ASSERT_FALSE(map.contains(1));
    ASSERT_EQ(0, destructNumber);

    map.remove(2);
    ASSERT_FALSE(map.contains(2));
    ASSERT_EQ(0UL, map.size());
    ASSERT_EQ(1, destructNumber);

    ownCounter1.clear();
    ASSERT_EQ(2, destructNumber);
}


class DummyRefCounted: public WTF::RefCounted<DummyRefCounted> {
public:
    DummyRefCounted(bool& isDeleted) : m_isDeleted(isDeleted) { m_isDeleted = false; }
    ~DummyRefCounted() { m_isDeleted = true; }

    void ref()
    {
        WTF::RefCounted<DummyRefCounted>::ref();
        ++m_refInvokesCount;
    }

    static int m_refInvokesCount;

private:
    bool& m_isDeleted;
};

int DummyRefCounted::m_refInvokesCount = 0;

TEST(WTF, HashMapWithRefPtrAsKey)
{
    bool isDeleted;
    RefPtr<DummyRefCounted> ptr = adoptRef(new DummyRefCounted(isDeleted));
    ASSERT_EQ(0, DummyRefCounted::m_refInvokesCount);
    HashMap<RefPtr<DummyRefCounted>, int> map;
    map.add(ptr, 1);
    // Referenced only once (to store a copy in the container).
    ASSERT_EQ(1, DummyRefCounted::m_refInvokesCount);
    ASSERT_EQ(1, map.get(ptr));

    DummyRefCounted* rawPtr = ptr.get();

    ASSERT_TRUE(map.contains(rawPtr));
    ASSERT_NE(map.end(), map.find(rawPtr));
    ASSERT_TRUE(map.contains(ptr));
    ASSERT_NE(map.end(), map.find(ptr));
    ASSERT_EQ(1, DummyRefCounted::m_refInvokesCount);

    ptr.clear();
    ASSERT_FALSE(isDeleted);

    map.remove(rawPtr);
    ASSERT_EQ(1, DummyRefCounted::m_refInvokesCount);
    ASSERT_TRUE(isDeleted);
    ASSERT_TRUE(map.isEmpty());
}

class SimpleClass {
public:
    explicit SimpleClass(int v) : m_v(v) { }
    int v() { return m_v; }

private:
    int m_v;
};
typedef HashMap<int, OwnPtr<SimpleClass> > IntSimpleMap;

TEST(HashMapTest, AddResult)
{
    IntSimpleMap map;
    IntSimpleMap::AddResult result = map.add(1, nullptr);
    EXPECT_TRUE(result.isNewEntry);
    EXPECT_EQ(1, result.storedValue->key);
    EXPECT_EQ(0, result.storedValue->value.get());

    SimpleClass* simple1 = new SimpleClass(1);
    result.storedValue->value = adoptPtr(simple1);
    EXPECT_EQ(simple1, map.get(1));

    IntSimpleMap::AddResult result2 = map.add(1, adoptPtr(new SimpleClass(2)));
    EXPECT_FALSE(result2.isNewEntry);
    EXPECT_EQ(1, map.get(1)->v());
}

} // namespace
