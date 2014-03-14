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

#include "wtf/ListHashSet.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"
#include <gtest/gtest.h>

namespace {

TEST(WTF, ListHashSetRemoveFirst)
{
    ListHashSet<int> list;
    list.add(1);
    list.add(2);
    list.add(3);

    ASSERT_EQ(1, list.first());

    list.removeFirst();
    ASSERT_EQ(2, list.first());

    list.removeFirst();
    ASSERT_EQ(3, list.first());

    list.removeFirst();
    ASSERT_TRUE(list.isEmpty());
}

TEST(WTF, ListHashSetAppendOrMoveToLastNewItems)
{
    ListHashSet<int> list;
    ListHashSet<int>::AddResult result = list.appendOrMoveToLast(1);
    ASSERT_TRUE(result.isNewEntry);
    result = list.add(2);
    ASSERT_TRUE(result.isNewEntry);
    result = list.appendOrMoveToLast(3);
    ASSERT_TRUE(result.isNewEntry);

    ASSERT_EQ(list.size(), 3UL);

    // The list should be in order 1, 2, 3.
    ListHashSet<int>::iterator iterator = list.begin();
    ASSERT_EQ(1, *iterator);
    ++iterator;
    ASSERT_EQ(2, *iterator);
    ++iterator;
    ASSERT_EQ(3, *iterator);
    ++iterator;
}

TEST(WTF, ListHashSetAppendOrMoveToLastWithDuplicates)
{
    ListHashSet<int> list;

    // Add a single element twice.
    ListHashSet<int>::AddResult result = list.add(1);
    ASSERT_TRUE(result.isNewEntry);
    result = list.appendOrMoveToLast(1);
    ASSERT_FALSE(result.isNewEntry);
    ASSERT_EQ(1UL, list.size());

    list.add(2);
    list.add(3);
    ASSERT_EQ(3UL, list.size());

    // Appending 2 move it to the end.
    ASSERT_EQ(3, list.last());
    result = list.appendOrMoveToLast(2);
    ASSERT_FALSE(result.isNewEntry);
    ASSERT_EQ(2, list.last());

    // Inverse the list by moving each element to end end.
    result = list.appendOrMoveToLast(3);
    ASSERT_FALSE(result.isNewEntry);
    result = list.appendOrMoveToLast(2);
    ASSERT_FALSE(result.isNewEntry);
    result = list.appendOrMoveToLast(1);
    ASSERT_FALSE(result.isNewEntry);
    ASSERT_EQ(3UL, list.size());

    ListHashSet<int>::iterator iterator = list.begin();
    ASSERT_EQ(3, *iterator);
    ++iterator;
    ASSERT_EQ(2, *iterator);
    ++iterator;
    ASSERT_EQ(1, *iterator);
    ++iterator;
}

TEST(WTF, ListHashSetPrependOrMoveToLastNewItems)
{
    ListHashSet<int> list;
    ListHashSet<int>::AddResult result = list.prependOrMoveToFirst(1);
    ASSERT_TRUE(result.isNewEntry);
    result = list.add(2);
    ASSERT_TRUE(result.isNewEntry);
    result = list.prependOrMoveToFirst(3);
    ASSERT_TRUE(result.isNewEntry);

    ASSERT_EQ(list.size(), 3UL);

    // The list should be in order 3, 1, 2.
    ListHashSet<int>::iterator iterator = list.begin();
    ASSERT_EQ(3, *iterator);
    ++iterator;
    ASSERT_EQ(1, *iterator);
    ++iterator;
    ASSERT_EQ(2, *iterator);
    ++iterator;
}

TEST(WTF, ListHashSetPrependOrMoveToLastWithDuplicates)
{
    ListHashSet<int> list;

    // Add a single element twice.
    ListHashSet<int>::AddResult result = list.add(1);
    ASSERT_TRUE(result.isNewEntry);
    result = list.prependOrMoveToFirst(1);
    ASSERT_FALSE(result.isNewEntry);
    ASSERT_EQ(1UL, list.size());

    list.add(2);
    list.add(3);
    ASSERT_EQ(3UL, list.size());

    // Prepending 2 move it to the beginning.
    ASSERT_EQ(1, list.first());
    result = list.prependOrMoveToFirst(2);
    ASSERT_FALSE(result.isNewEntry);
    ASSERT_EQ(2, list.first());

    // Inverse the list by moving each element to the first position.
    result = list.prependOrMoveToFirst(1);
    ASSERT_FALSE(result.isNewEntry);
    result = list.prependOrMoveToFirst(2);
    ASSERT_FALSE(result.isNewEntry);
    result = list.prependOrMoveToFirst(3);
    ASSERT_FALSE(result.isNewEntry);
    ASSERT_EQ(3UL, list.size());

    ListHashSet<int>::iterator iterator = list.begin();
    ASSERT_EQ(3, *iterator);
    ++iterator;
    ASSERT_EQ(2, *iterator);
    ++iterator;
    ASSERT_EQ(1, *iterator);
    ++iterator;
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

TEST(WTF, ListHashSetWithRefPtr)
{
    bool isDeleted;
    RefPtr<DummyRefCounted> ptr = adoptRef(new DummyRefCounted(isDeleted));
    ASSERT_EQ(0, DummyRefCounted::m_refInvokesCount);

    ListHashSet<RefPtr<DummyRefCounted> > list;
    list.add(ptr);
    // Referenced only once (to store a copy in the container).
    ASSERT_EQ(1, DummyRefCounted::m_refInvokesCount);
    ASSERT_EQ(ptr, list.first());

    DummyRefCounted* rawPtr = ptr.get();

    ASSERT_TRUE(list.contains(ptr));
    ASSERT_TRUE(list.contains(rawPtr));

    ptr.clear();
    ASSERT_FALSE(isDeleted);

    list.remove(rawPtr);
    ASSERT_TRUE(isDeleted);

    ASSERT_EQ(1, DummyRefCounted::m_refInvokesCount);
}

} // namespace
