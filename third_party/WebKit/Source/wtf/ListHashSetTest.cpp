/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#include "wtf/LinkedHashSet.h"
#include "wtf/ListHashSet.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"
#include <gtest/gtest.h>

namespace {

template<typename Set>
void removeFirstHelper()
{
    Set list;
    list.add(-1);
    list.add(0);
    list.add(1);
    list.add(2);
    list.add(3);

    EXPECT_EQ(-1, list.first());
    EXPECT_EQ(3, list.last());

    list.removeFirst();
    EXPECT_EQ(0, list.first());

    list.removeLast();
    EXPECT_EQ(2, list.last());

    list.removeFirst();
    EXPECT_EQ(1, list.first());

    list.removeFirst();
    EXPECT_EQ(2, list.first());

    list.removeFirst();
    EXPECT_TRUE(list.isEmpty());
}

TEST(WTF, ListHashSetRemoveFirst)
{
    removeFirstHelper<ListHashSet<int> >();
    removeFirstHelper<ListHashSet<int, 1> >();
}

TEST(WTF, LinkedHashSetRemoveFirst)
{
    removeFirstHelper<LinkedHashSet<int> >();
}

template<typename Set>
void appendOrMoveToLastNewItems()
{
    Set list;
    typename Set::AddResult result = list.appendOrMoveToLast(1);
    EXPECT_TRUE(result.isNewEntry);
    result = list.add(2);
    EXPECT_TRUE(result.isNewEntry);
    result = list.appendOrMoveToLast(3);
    EXPECT_TRUE(result.isNewEntry);

    EXPECT_EQ(list.size(), 3UL);

    // The list should be in order 1, 2, 3.
    typename Set::iterator iterator = list.begin();
    EXPECT_EQ(1, *iterator);
    ++iterator;
    EXPECT_EQ(2, *iterator);
    ++iterator;
    EXPECT_EQ(3, *iterator);
    ++iterator;
}

TEST(WTF, ListHashSetAppendOrMoveToLastNewItems)
{
    appendOrMoveToLastNewItems<ListHashSet<int> >();
    appendOrMoveToLastNewItems<ListHashSet<int, 1> >();
}

TEST(WTF, LinkedHashSetAppendOrMoveToLastNewItems)
{
    appendOrMoveToLastNewItems<LinkedHashSet<int> >();
}

template<typename Set>
void appendOrMoveToLastWithDuplicates()
{
    Set list;

    // Add a single element twice.
    typename Set::AddResult result = list.add(1);
    EXPECT_TRUE(result.isNewEntry);
    result = list.appendOrMoveToLast(1);
    EXPECT_FALSE(result.isNewEntry);
    EXPECT_EQ(1UL, list.size());

    list.add(2);
    list.add(3);
    EXPECT_EQ(3UL, list.size());

    // Appending 2 move it to the end.
    EXPECT_EQ(3, list.last());
    result = list.appendOrMoveToLast(2);
    EXPECT_FALSE(result.isNewEntry);
    EXPECT_EQ(2, list.last());

    // Inverse the list by moving each element to end end.
    result = list.appendOrMoveToLast(3);
    EXPECT_FALSE(result.isNewEntry);
    result = list.appendOrMoveToLast(2);
    EXPECT_FALSE(result.isNewEntry);
    result = list.appendOrMoveToLast(1);
    EXPECT_FALSE(result.isNewEntry);
    EXPECT_EQ(3UL, list.size());

    typename Set::iterator iterator = list.begin();
    EXPECT_EQ(3, *iterator);
    ++iterator;
    EXPECT_EQ(2, *iterator);
    ++iterator;
    EXPECT_EQ(1, *iterator);
    ++iterator;
}

TEST(WTF, ListHashSetAppendOrMoveToLastWithDuplicates)
{
    appendOrMoveToLastWithDuplicates<ListHashSet<int> >();
    appendOrMoveToLastWithDuplicates<ListHashSet<int, 1> >();
}

TEST(WTF, LinkedHashSetAppendOrMoveToLastWithDuplicates)
{
    appendOrMoveToLastWithDuplicates<LinkedHashSet<int> >();
}

template<typename Set>
void prependOrMoveToFirstNewItems()
{
    Set list;
    typename Set::AddResult result = list.prependOrMoveToFirst(1);
    EXPECT_TRUE(result.isNewEntry);
    result = list.add(2);
    EXPECT_TRUE(result.isNewEntry);
    result = list.prependOrMoveToFirst(3);
    EXPECT_TRUE(result.isNewEntry);

    EXPECT_EQ(list.size(), 3UL);

    // The list should be in order 3, 1, 2.
    typename Set::iterator iterator = list.begin();
    EXPECT_EQ(3, *iterator);
    ++iterator;
    EXPECT_EQ(1, *iterator);
    ++iterator;
    EXPECT_EQ(2, *iterator);
    ++iterator;
}

TEST(WTF, ListHashSetPrependOrMoveToFirstNewItems)
{
    prependOrMoveToFirstNewItems<ListHashSet<int> >();
    prependOrMoveToFirstNewItems<ListHashSet<int, 1> >();
}

TEST(WTF, LinkedHashSetPrependOrMoveToFirstNewItems)
{
    prependOrMoveToFirstNewItems<LinkedHashSet<int> >();
}

template<typename Set>
void prependOrMoveToLastWithDuplicates()
{
    Set list;

    // Add a single element twice.
    typename Set::AddResult result = list.add(1);
    EXPECT_TRUE(result.isNewEntry);
    result = list.prependOrMoveToFirst(1);
    EXPECT_FALSE(result.isNewEntry);
    EXPECT_EQ(1UL, list.size());

    list.add(2);
    list.add(3);
    EXPECT_EQ(3UL, list.size());

    // Prepending 2 move it to the beginning.
    EXPECT_EQ(1, list.first());
    result = list.prependOrMoveToFirst(2);
    EXPECT_FALSE(result.isNewEntry);
    EXPECT_EQ(2, list.first());

    // Inverse the list by moving each element to the first position.
    result = list.prependOrMoveToFirst(1);
    EXPECT_FALSE(result.isNewEntry);
    result = list.prependOrMoveToFirst(2);
    EXPECT_FALSE(result.isNewEntry);
    result = list.prependOrMoveToFirst(3);
    EXPECT_FALSE(result.isNewEntry);
    EXPECT_EQ(3UL, list.size());

    typename Set::iterator iterator = list.begin();
    EXPECT_EQ(3, *iterator);
    ++iterator;
    EXPECT_EQ(2, *iterator);
    ++iterator;
    EXPECT_EQ(1, *iterator);
    ++iterator;
}

TEST(WTF, ListHashSetPrependOrMoveToLastWithDuplicates)
{
    prependOrMoveToLastWithDuplicates<ListHashSet<int> >();
    prependOrMoveToLastWithDuplicates<ListHashSet<int, 1> >();
}

TEST(WTF, LinkedHashSetPrependOrMoveToLastWithDuplicates)
{
    prependOrMoveToLastWithDuplicates<LinkedHashSet<int> >();
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

template<typename Set>
void withRefPtr()
{
    bool isDeleted;
    DummyRefCounted::m_refInvokesCount = 0;
    RefPtr<DummyRefCounted> ptr = adoptRef(new DummyRefCounted(isDeleted));
    EXPECT_EQ(0, DummyRefCounted::m_refInvokesCount);

    Set set;
    set.add(ptr);
    // Referenced only once (to store a copy in the container).
    EXPECT_EQ(1, DummyRefCounted::m_refInvokesCount);
    EXPECT_EQ(ptr, set.first());
    EXPECT_EQ(1, DummyRefCounted::m_refInvokesCount);

    DummyRefCounted* rawPtr = ptr.get();

    EXPECT_TRUE(set.contains(ptr));
    EXPECT_TRUE(set.contains(rawPtr));
    EXPECT_EQ(1, DummyRefCounted::m_refInvokesCount);

    ptr.clear();
    EXPECT_FALSE(isDeleted);
    EXPECT_EQ(1, DummyRefCounted::m_refInvokesCount);

    set.remove(rawPtr);
    EXPECT_TRUE(isDeleted);

    EXPECT_EQ(1, DummyRefCounted::m_refInvokesCount);
}

TEST(WTF, ListHashSetWithRefPtr)
{
    withRefPtr<ListHashSet<RefPtr<DummyRefCounted> > >();
    withRefPtr<ListHashSet<RefPtr<DummyRefCounted>, 1> >();
}

TEST(WTF, LinkedHashSetWithRefPtr)
{
    withRefPtr<LinkedHashSet<RefPtr<DummyRefCounted> > >();
}

template<typename Set, typename SetRef, typename Iterator>
void findHelper()
{
    Set set;
    set.add(-1);
    set.add(0);
    set.add(1);
    set.add(2);
    set.add(3);

    SetRef ref = set;
    Iterator it = ref.find(2);
    EXPECT_EQ(2, *it);
    ++it;
    EXPECT_EQ(3, *it);
    --it;
    --it;
    EXPECT_EQ(1, *it);
}

TEST(WTF, ListHashSetFind)
{
    findHelper<ListHashSet<int>, const ListHashSet<int>&, ListHashSet<int>::const_iterator>();
    findHelper<ListHashSet<int>, ListHashSet<int>&, ListHashSet<int>::iterator>();
    findHelper<ListHashSet<int, 1>, const ListHashSet<int, 1>&, ListHashSet<int, 1>::const_iterator>();
    findHelper<ListHashSet<int, 1>, ListHashSet<int, 1>&, ListHashSet<int, 1>::iterator>();
}

TEST(WTF, LinkedHashSetFind)
{
    findHelper<LinkedHashSet<int>, const LinkedHashSet<int>&, LinkedHashSet<int>::const_iterator>();
    findHelper<LinkedHashSet<int>, LinkedHashSet<int>&, LinkedHashSet<int>::iterator>();
}

template<typename Set>
void insertBeforeHelper(bool canModifyWhileIterating)
{
    Set set;
    set.add(-1);
    set.add(0);
    set.add(2);
    set.add(3);

    typename Set::iterator it = set.find(2);
    EXPECT_EQ(2, *it);
    set.insertBefore(it, 1);
    if (!canModifyWhileIterating)
        it = set.find(2);
    ++it;
    EXPECT_EQ(3, *it);
    EXPECT_EQ(5u, set.size());
    --it;
    --it;
    EXPECT_EQ(1, *it);
    if (canModifyWhileIterating) {
        set.remove(-1);
        set.remove(0);
        set.remove(2);
        set.remove(3);
        EXPECT_EQ(1u, set.size());
        EXPECT_EQ(1, *it);
        ++it;
        EXPECT_EQ(it, set.end());
        --it;
        EXPECT_EQ(1, *it);
        set.insertBefore(it, -1);
        set.insertBefore(it, 0);
        set.add(2);
        set.add(3);
    }
    set.insertBefore(2, 42);
    set.insertBefore(-1, 103);
    EXPECT_EQ(103, set.first());
    if (!canModifyWhileIterating)
        it = set.find(1);
    ++it;
    EXPECT_EQ(42, *it);
    EXPECT_EQ(7u, set.size());
}

TEST(WTF, ListHashSetInsertBefore)
{
    insertBeforeHelper<ListHashSet<int> >(true);
    insertBeforeHelper<ListHashSet<int, 1> >(true);
}

TEST(WTF, LinkedHashSetInsertBefore)
{
    insertBeforeHelper<LinkedHashSet<int> >(false);
}

template<typename Set>
void addReturnIterator(bool canModifyWhileIterating)
{
    Set set;
    set.add(-1);
    set.add(0);
    set.add(1);
    set.add(2);

    typename Set::iterator it = set.addReturnIterator(3);
    EXPECT_EQ(3, *it);
    --it;
    EXPECT_EQ(2, *it);
    EXPECT_EQ(5u, set.size());
    --it;
    EXPECT_EQ(1, *it);
    --it;
    EXPECT_EQ(0, *it);
    it = set.addReturnIterator(4);
    if (canModifyWhileIterating) {
        set.remove(3);
        set.remove(2);
        set.remove(1);
        set.remove(0);
        set.remove(-1);
        EXPECT_EQ(1u, set.size());
    }
    EXPECT_EQ(4, *it);
    ++it;
    EXPECT_EQ(it, set.end());
    --it;
    EXPECT_EQ(4, *it);
    if (canModifyWhileIterating) {
        set.insertBefore(it, -1);
        set.insertBefore(it, 0);
        set.insertBefore(it, 1);
        set.insertBefore(it, 2);
        set.insertBefore(it, 3);
    }
    EXPECT_EQ(6u, set.size());
    it = set.addReturnIterator(5);
    EXPECT_EQ(7u, set.size());
    set.remove(it);
    EXPECT_EQ(6u, set.size());
    EXPECT_EQ(4, set.last());
}

TEST(WTF, ListHashSetAddReturnIterator)
{
    addReturnIterator<ListHashSet<int> >(true);
    addReturnIterator<ListHashSet<int, 1> >(true);
}

TEST(WTF, LinkedHashSetAddReturnIterator)
{
    addReturnIterator<LinkedHashSet<int> >(false);
}

template<typename Set>
void excerciseValuePeekInType()
{
    Set set;
    bool isDeleted = false;
    bool isDeleted2 = false;

    RefPtr<DummyRefCounted> ptr = adoptRef(new DummyRefCounted(isDeleted));
    RefPtr<DummyRefCounted> ptr2 = adoptRef(new DummyRefCounted(isDeleted2));

    typename Set::AddResult addResult = set.add(ptr);
    EXPECT_TRUE(addResult.isNewEntry);
    set.find(ptr);
    const Set& constSet(set);
    constSet.find(ptr);
    EXPECT_TRUE(set.contains(ptr));
    typename Set::iterator it = set.addReturnIterator(ptr);
    set.appendOrMoveToLast(ptr);
    set.prependOrMoveToFirst(ptr);
    set.insertBefore(ptr, ptr);
    set.insertBefore(it, ptr);
    EXPECT_EQ(1u, set.size());
    set.add(ptr2);
    ptr2.clear();
    set.remove(ptr);

    EXPECT_FALSE(isDeleted);
    ptr.clear();
    EXPECT_TRUE(isDeleted);

    EXPECT_FALSE(isDeleted2);
    set.removeFirst();
    EXPECT_TRUE(isDeleted2);

    EXPECT_EQ(0u, set.size());
}

TEST(WTF, ListHashSetExcerciseValuePeekInType)
{
    excerciseValuePeekInType<ListHashSet<RefPtr<DummyRefCounted> > >();
    excerciseValuePeekInType<ListHashSet<RefPtr<DummyRefCounted>, 1> >();
}

TEST(WTF, LinkedHashSetExcerciseValuePeekInType)
{
    excerciseValuePeekInType<LinkedHashSet<RefPtr<DummyRefCounted> > >();
}

struct Simple {
    Simple(int value) : m_value(value) { };
    int m_value;
};

struct Complicated {
    Complicated(int value) : m_simple(value)
    {
        s_objectsConstructed++;
    }

    Complicated(const Complicated& other) : m_simple(other.m_simple)
    {
        s_objectsConstructed++;
    }

    Simple m_simple;
    static int s_objectsConstructed;

private:
    Complicated();
};

int Complicated::s_objectsConstructed = 0;

struct ComplicatedHashFunctions {
    static unsigned hash(const Complicated& key) { return key.m_simple.m_value; }
    static bool equal(const Complicated& a, const Complicated& b) { return a.m_simple.m_value == b.m_simple.m_value; }
};
struct ComplexityTranslator {
    static unsigned hash(const Simple& key) { return key.m_value; }
    static bool equal(const Complicated& a, const Simple& b) { return a.m_simple.m_value == b.m_value; }
};

template<typename Set>
void translatorTest()
{
    Set set;
    set.add(Complicated(42));
    int baseLine = Complicated::s_objectsConstructed;

    typename Set::iterator it = set.template find<ComplexityTranslator>(Simple(42));
    EXPECT_NE(it, set.end());
    EXPECT_EQ(baseLine, Complicated::s_objectsConstructed);

    it = set.template find<ComplexityTranslator>(Simple(103));
    EXPECT_EQ(it, set.end());
    EXPECT_EQ(baseLine, Complicated::s_objectsConstructed);

    const Set& constSet(set);

    typename Set::const_iterator constIterator = constSet.template find<ComplexityTranslator>(Simple(42));
    EXPECT_NE(constIterator, constSet.end());
    EXPECT_EQ(baseLine, Complicated::s_objectsConstructed);

    constIterator = constSet.template find<ComplexityTranslator>(Simple(103));
    EXPECT_EQ(constIterator, constSet.end());
    EXPECT_EQ(baseLine, Complicated::s_objectsConstructed);
}

TEST(WTF, ListHashSetComplexityTranslator)
{
    translatorTest<ListHashSet<Complicated, 256, ComplicatedHashFunctions> >();
    translatorTest<ListHashSet<Complicated, 1, ComplicatedHashFunctions> >();
}

TEST(WTF, LinkedHashSetComplexityTranslator)
{
    translatorTest<LinkedHashSet<Complicated, ComplicatedHashFunctions> >();
}

} // namespace
