/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "config.h"

#include "heap/Handle.h"
#include "heap/Heap.h"
#include "heap/ThreadState.h"

#include <gtest/gtest.h>

using namespace WebCore;

namespace {

class HeapAllocatedArray : public GarbageCollected<HeapAllocatedArray> {
    DECLARE_GC_INFO
public:
    HeapAllocatedArray()
    {
        for (int i = 0; i < s_arraySize; ++i) {
            m_array[i] = i % 128;
        }
    }

    int8_t at(size_t i) { return m_array[i]; }
    void trace(Visitor*) { }
private:
    static const int s_arraySize = 1000;
    int8_t m_array[s_arraySize];
};

DEFINE_GC_INFO(HeapAllocatedArray);

TEST(HeapTest, Init)
{
    // FIXME: init and shutdown should be called via Blink
    // initialization in the test runner. This test can be removed
    // when that is done.
    Heap::init(0);
    Heap::shutdown();
}

TEST(HeapTest, SimpleAllocation)
{
    // FIXME: init and shutdown should be called via Blink
    // initialization in the test runner.
    Heap::init(0);

    // Get initial heap stats.
    //
    // FIXME: We should put in a testing GCScope object that will make
    // the test thread reach a safepoint so that we can enable
    // threading asserts in getStats.
    HeapStats initialHeapStats;
    Heap::getStats(&initialHeapStats);
    EXPECT_EQ(0ul, initialHeapStats.totalObjectSpace());

    // Allocate an object in the heap.
    HeapAllocatedArray* a = new HeapAllocatedArray();
    HeapStats statsAfterAllocation;
    Heap::getStats(&statsAfterAllocation);
    EXPECT_TRUE(statsAfterAllocation.totalObjectSpace() >= sizeof(HeapAllocatedArray));

    // Sanity check of the contents in the heap.
    EXPECT_EQ(0, a->at(0));
    EXPECT_EQ(42, a->at(42));
    EXPECT_EQ(0, a->at(128));
    EXPECT_EQ(999 % 128, a->at(999));

    Heap::shutdown();
}

class TraceCounter : public GarbageCollectedFinalized<TraceCounter> {
    DECLARE_GC_INFO
public:
    static TraceCounter* create()
    {
        return new TraceCounter();
    }

    void trace(Visitor*) { m_traceCount++; }

    int traceCount() { return m_traceCount; }

private:
    TraceCounter()
        : m_traceCount(0)
    {
    }

    int m_traceCount;
};

DEFINE_GC_INFO(TraceCounter);

class ClassWithMember : public GarbageCollected<ClassWithMember> {
    DECLARE_GC_INFO
public:
    static ClassWithMember* create()
    {
        return new ClassWithMember();
    }

    void trace(Visitor* visitor) { visitor->trace(m_traceCounter); }

    int traceCount() { return m_traceCounter->traceCount(); }

private:
    ClassWithMember()
        : m_traceCounter(TraceCounter::create())
    { }

    Member<TraceCounter> m_traceCounter;
};

DEFINE_GC_INFO(ClassWithMember);

TEST(HeapTest, SimplePersistent)
{
    // FIXME: init and shutdown should be called via Blink
    // initialization in the test runner.
    Heap::init(0);

    Persistent<TraceCounter> traceCounter = TraceCounter::create();
    EXPECT_EQ(0, traceCounter->traceCount());

    Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
    EXPECT_EQ(1, traceCounter->traceCount());

    Persistent<ClassWithMember> classWithMember = ClassWithMember::create();
    EXPECT_EQ(0, classWithMember->traceCount());

    Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
    EXPECT_EQ(1, classWithMember->traceCount());
    EXPECT_EQ(2, traceCounter->traceCount());

    Heap::shutdown();
}

} // namespace
