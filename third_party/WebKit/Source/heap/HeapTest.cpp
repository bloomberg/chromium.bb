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

namespace WebCore {

class TestGCScope {
public:
    explicit TestGCScope(ThreadState::StackState state)
        : m_state(ThreadState::current())
        , m_safePointScope(state)
    {
        m_state->checkThread();
        ASSERT(!m_state->isInGC());
        ThreadState::stopThreads();
        m_state->enterGC();
        Heap::makeConsistentForGC();
    }

    ~TestGCScope()
    {
        m_state->leaveGC();
        ASSERT(!m_state->isInGC());
        ThreadState::resumeThreads();
    }

private:
    ThreadState* m_state;
    ThreadState::SafePointScope m_safePointScope;
};

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

class IntWrapper : public GarbageCollectedFinalized<IntWrapper> {
    DECLARE_GC_INFO
public:
    static IntWrapper* create(int x)
    {
        return new IntWrapper(x);
    }

    virtual ~IntWrapper()
    {
        ++s_destructorCalls;
    }

    static int s_destructorCalls;

    void trace(Visitor*) { }
    int value() const { return m_x; }
    bool operator==(const IntWrapper& other) const { return other.value() == value(); }
    unsigned hash() { return IntHash<int>::hash(m_x); }

protected:
    IntWrapper(int x) : m_x(x) { }

private:
    IntWrapper();
    int m_x;
};

DEFINE_GC_INFO(IntWrapper);
USED_FROM_MULTIPLE_THREADS(IntWrapper);

int IntWrapper::s_destructorCalls = 0;

class ThreadedHeapTester {
public:
    static void test()
    {
        ThreadedHeapTester* tester = new ThreadedHeapTester();
        for (int i = 0; i < numberOfThreads; i++)
            createThread(&threadFunc, tester, "testing thread");
        while (tester->m_threadsToFinish) {
            ThreadState::current()->safePoint();
            yield();
        }
    }

private:
    static const int numberOfThreads = 10;
    static const int gcPerThread = 5;
    static const int numberOfAllocations = 50;

    inline bool done() const { return m_gcCount >= numberOfThreads * gcPerThread; }

    ThreadedHeapTester() : m_gcCount(0), m_threadsToFinish(numberOfThreads)
    {
    }

    static void threadFunc(void* data)
    {
        intptr_t stackMarker;
        reinterpret_cast<ThreadedHeapTester*>(data)->runThread(&stackMarker);
    }

    void runThread(intptr_t* startOfStack)
    {
        ThreadState::attach(startOfStack);

        int gcCount = 0;
        while (!done()) {
            ThreadState::current()->safePoint();
            {
                // FIXME: Just use a raw pointer here when we know the
                // start of the stack and can use conservative stack
                // scanning.
                Persistent<IntWrapper> wrapper;

                for (int i = 0; i < numberOfAllocations; i++) {
                    wrapper = IntWrapper::create(0x0bbac0de);
                    if (!(i % 10))
                        ThreadState::current()->safePoint();
                    yield();
                }

                if (gcCount < gcPerThread) {
                    // FIXME: Use HeapPointersOnStack here when we
                    // know the start of the stack and can use
                    // conservative stack scanning.
                    Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
                    gcCount++;
                    atomicIncrement(&m_gcCount);
                }

                EXPECT_EQ(wrapper->value(), 0x0bbac0de);
            }
            yield();
        }
        ThreadState::detach();
        atomicDecrement(&m_threadsToFinish);
    }

    volatile int m_gcCount;
    volatile int m_threadsToFinish;
};

static void getHeapStats(HeapStats* stats)
{
    TestGCScope scope(ThreadState::NoHeapPointersOnStack);
    Heap::getStats(stats);
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

    void trace(Visitor* visitor)
    {
        EXPECT_TRUE(visitor->isMarked(this));
        if (!traceCount())
            EXPECT_FALSE(visitor->isMarked(m_traceCounter));
        else
            EXPECT_TRUE(visitor->isMarked(m_traceCounter));

        visitor->trace(m_traceCounter);
    }

    int traceCount() { return m_traceCounter->traceCount(); }

private:
    ClassWithMember()
        : m_traceCounter(TraceCounter::create())
    { }

    Member<TraceCounter> m_traceCounter;
};

DEFINE_GC_INFO(ClassWithMember);

class SimpleFinalizedObject : public GarbageCollectedFinalized<SimpleFinalizedObject> {
    DECLARE_GC_INFO
public:
    static SimpleFinalizedObject* create()
    {
        return new SimpleFinalizedObject();
    }

    ~SimpleFinalizedObject()
    {
        ++s_destructorCalls;
    }

    static int s_destructorCalls;

    void trace(Visitor*) { }

private:
    SimpleFinalizedObject() { }
};

DEFINE_GC_INFO(SimpleFinalizedObject);
int SimpleFinalizedObject::s_destructorCalls = 0;

class TestTypedHeapClass : public GarbageCollected<TestTypedHeapClass> {
    DECLARE_GC_INFO
public:
    static TestTypedHeapClass* create()
    {
        return new TestTypedHeapClass();
    }

    void trace(Visitor*) { }

private:
    TestTypedHeapClass() { }
};

DEFINE_GC_INFO(TestTypedHeapClass);

class Bar : public GarbageCollectedFinalized<Bar> {
    DECLARE_GC_INFO
public:
    static Bar* create()
    {
        return new Bar();
    }

    void finalize()
    {
        EXPECT_TRUE(m_magic == magic);
        m_magic = 0;
        s_live--;
    }

    virtual void trace(Visitor* visitor) { }
    static unsigned s_live;

protected:
    static const int magic = 1337;
    int m_magic;

    Bar()
        : m_magic(magic)
    {
        s_live++;
    }
};

DEFINE_GC_INFO(Bar);
unsigned Bar::s_live = 0;

class Baz : public GarbageCollected<Baz> {
    DECLARE_GC_INFO
public:
    static Baz* create(Bar* bar)
    {
        return new Baz(bar);
    }

    void trace(Visitor* visitor)
    {
        visitor->trace(m_bar);
    }

    void clear() { m_bar.clear(); }

private:
    explicit Baz(Bar* bar)
        : m_bar(bar)
    {
    }

    Member<Bar> m_bar;
};

DEFINE_GC_INFO(Baz);

class Foo : public Bar {
public:
    static Foo* create(Bar* bar)
    {
        return new Foo(bar);
    }

    static Foo* create(Foo* foo)
    {
        return new Foo(foo);
    }

    virtual void trace(Visitor* visitor)
    {
        if (m_pointsToFoo)
            visitor->mark(static_cast<Foo*>(m_bar));
        else
            visitor->mark(m_bar);
    }

private:
    Foo(Bar* bar)
        : Bar()
        , m_bar(bar)
        , m_pointsToFoo(false)
    {
    }

    Foo(Foo* foo)
        : Bar()
        , m_bar(foo)
        , m_pointsToFoo(true)
    {
    }

    Bar* m_bar;
    bool m_pointsToFoo;
};

class Bars : public Bar {
public:
    static Bars* create()
    {
        return new Bars();
    }

    virtual void trace(Visitor* visitor)
    {
        for (unsigned i = 0; i < m_width; i++)
            visitor->trace(m_bars[i]);
    }

    unsigned getWidth() const
    {
        return m_width;
    }

    static const unsigned width = 7500;
private:
    Bars() : m_width(0)
    {
        for (unsigned i = 0; i < width; i++) {
            m_bars[i] = Bar::create();
            m_width++;
        }
    }

    unsigned m_width;
    Member<Bar> m_bars[width];
};

TEST(HeapTest, Threading)
{
    Heap::init(0);
    ThreadedHeapTester::test();
    Heap::shutdown();
}

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
    HeapStats initialHeapStats;
    getHeapStats(&initialHeapStats);
    EXPECT_EQ(0ul, initialHeapStats.totalObjectSpace());

    // Allocate an object in the heap.
    HeapAllocatedArray* array = new HeapAllocatedArray();
    HeapStats statsAfterAllocation;
    getHeapStats(&statsAfterAllocation);
    EXPECT_TRUE(statsAfterAllocation.totalObjectSpace() >= sizeof(HeapAllocatedArray));

    // Sanity check of the contents in the heap.
    EXPECT_EQ(0, array->at(0));
    EXPECT_EQ(42, array->at(42));
    EXPECT_EQ(0, array->at(128));
    EXPECT_EQ(999 % 128, array->at(999));

    Heap::shutdown();
}

TEST(HeapTest, SimplePersistent)
{
    // FIXME: init and shutdown should be called via Blink
    // initialization in the test runner.
    Heap::init(0);

    {
        Persistent<TraceCounter> traceCounter = TraceCounter::create();
        EXPECT_EQ(0, traceCounter->traceCount());

        Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
        EXPECT_EQ(1, traceCounter->traceCount());

        Persistent<ClassWithMember> classWithMember = ClassWithMember::create();
        EXPECT_EQ(0, classWithMember->traceCount());

        Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
        EXPECT_EQ(1, classWithMember->traceCount());
        EXPECT_EQ(2, traceCounter->traceCount());
    }

    Heap::shutdown();
}

TEST(HeapTest, SimpleFinalization)
{
    // FIXME: init and shutdown should be called via Blink
    // initialization in the test runner.
    Heap::init(0);

    {
        Persistent<SimpleFinalizedObject> finalized = SimpleFinalizedObject::create();
        EXPECT_EQ(0, SimpleFinalizedObject::s_destructorCalls);
        Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
        EXPECT_EQ(0, SimpleFinalizedObject::s_destructorCalls);
    }

    Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
    EXPECT_EQ(1, SimpleFinalizedObject::s_destructorCalls);

    Heap::shutdown();
}

TEST(HeapTest, TypedHeapSanity)
{
    // FIXME: init and shutdown should be called via Blink
    // initialization in the test runner.
    Heap::init(0);

    {
        // We use TraceCounter for allocating an object on the general heap.
        Persistent<TraceCounter> generalHeapObject = TraceCounter::create();
        Persistent<TestTypedHeapClass> typedHeapObject = TestTypedHeapClass::create();

        // FIXME: Add a check that these two objects are allocated on separate pages.
    }

    Heap::shutdown();
}

TEST(HeapTest, NoAllocation)
{
    // FIXME: init and shutdown should be called via Blink
    // initialization in the test runner.
    Heap::init(0);

    EXPECT_TRUE(ThreadState::current()->isAllocationAllowed());
    {
        // Disallow allocation
        NoAllocationScope<AnyThread> noAllocationScope;
        EXPECT_FALSE(ThreadState::current()->isAllocationAllowed());
    }
    EXPECT_TRUE(ThreadState::current()->isAllocationAllowed());

    Heap::shutdown();
}

TEST(HeapTest, Members)
{
    // FIXME: init and shutdown should be called via Blink
    // initialization in the test runner.
    Heap::init(0);

    Bar::s_live = 0;
    {
        Persistent<Baz> h1;
        Persistent<Baz> h2;
        {
            h1 = Baz::create(Bar::create());
            Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
            EXPECT_EQ(1u, Bar::s_live);
            h2 = Baz::create(Bar::create());
            Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
            EXPECT_EQ(2u, Bar::s_live);
        }
        Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
        EXPECT_EQ(2u, Bar::s_live);
        h1->clear();
        Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
        EXPECT_EQ(1u, Bar::s_live);
    }
    Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
    EXPECT_EQ(0u, Bar::s_live);

    Heap::shutdown();
}

TEST(HeapTest, DeepTest)
{
    // FIXME: init and shutdown should be called via Blink
    // initialization in the test runner.
    Heap::init(0);

    // FIXME: Increase depth to 100000 when conservative stack
    // scanning is implemented. Allocating 100000 things here
    // will lead to a forced conservative garbage collection.
    const unsigned depth = 100;
    Bar::s_live = 0;
    {
        Persistent<Bar> bar = Bar::create();
        EXPECT_TRUE(ThreadState::current()->contains(bar));
        Persistent<Foo> foo = Foo::create(bar);
        EXPECT_TRUE(ThreadState::current()->contains(foo));
        EXPECT_EQ(2u, Bar::s_live);
        for (unsigned i = 0; i < depth; i++) {
            Foo* foo2 = Foo::create(foo);
            foo = foo2;
            EXPECT_TRUE(ThreadState::current()->contains(foo));
        }
        EXPECT_EQ(depth + 2, Bar::s_live);
        Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
        EXPECT_EQ(depth + 2, Bar::s_live);
    }
    Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
    EXPECT_EQ(0u, Bar::s_live);

    Heap::shutdown();
}

TEST(HeapTest, WideTest)
{
    // FIXME: init and shutdown should be called via Blink
    // initialization in the test runner.
    Heap::init(0);

    Bar::s_live = 0;
    {
        // FIXME: Use raw pointer here when conservative stack
        // scanning works.
        Persistent<Bars> bars = Bars::create();
        unsigned width = Bars::width;
        EXPECT_EQ(width + 1, Bar::s_live);
        // FIXME: Use HeapPointersOnStack here when conservative stack
        // scanning works.
        Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
        EXPECT_EQ(width + 1, Bar::s_live);
        // Use bars here to make sure that it will be on the stack
        // for the conservative stack scan to find.
        EXPECT_EQ(width, bars->getWidth());
    }
    EXPECT_EQ(Bars::width + 1, Bar::s_live);
    Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
    EXPECT_EQ(0u, Bar::s_live);

    Heap::shutdown();
}

} // namespace
