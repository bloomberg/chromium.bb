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

static void getHeapStats(HeapStats* stats)
{
    TestGCScope scope(ThreadState::NoHeapPointersOnStack);
    Heap::getStats(stats);
}

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

// Do several GCs to make sure that later GCs don't free up old memory from
// previously run tests in this process.
static void clearOutOldGarbage(HeapStats* heapStats)
{
    while (true) {
        getHeapStats(heapStats);
        size_t used = heapStats->totalObjectSpace();
        Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
        getHeapStats(heapStats);
        if (heapStats->totalObjectSpace() >= used)
            break;
    }
}

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
    static void trace(Visitor*) { }

    int value() const { return m_x; }

    bool operator==(const IntWrapper& other) const { return other.value() == value(); }

    unsigned hash() { return IntHash<int>::hash(m_x); }

protected:
    IntWrapper(int x) : m_x(x) { }

private:
    IntWrapper();
    int m_x;
};

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
        reinterpret_cast<ThreadedHeapTester*>(data)->runThread();
    }

    void runThread()
    {
        ThreadState::attach();

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

// The accounting for memory includes the memory used by rounding up object
// sizes. This is done in a different way on 32 bit and 64 bit, so we have to
// have some slack in the tests.
template<typename T>
void CheckWithSlack(T expected, T actual, int slack)
{
    EXPECT_LE(expected, actual);
    EXPECT_GE((intptr_t)expected + slack, (intptr_t)actual);
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

class ConstructorAllocation : public GarbageCollected<ConstructorAllocation> {
    DECLARE_GC_INFO
public:
    static ConstructorAllocation* create() { return new ConstructorAllocation(); }

    void trace(Visitor* visitor) { visitor->trace(m_intWrapper); }

private:
    ConstructorAllocation()
    {
        m_intWrapper = IntWrapper::create(42);
    }

    Member<IntWrapper> m_intWrapper;
};

class LargeObject : public GarbageCollectedFinalized<LargeObject> {
    DECLARE_GC_INFO
public:
    ~LargeObject()
    {
        s_destructorCalls++;
    }
    static LargeObject* create() { return new LargeObject(); }
    char get(size_t i) { return m_data[i]; }
    void set(size_t i, char c) { m_data[i] = c; }
    size_t length() { return s_length; }
    void trace(Visitor* visitor)
    {
        visitor->trace(m_intWrapper);
    }
    static int s_destructorCalls;

private:
    static const size_t s_length = 1024*1024;
    LargeObject()
    {
        m_intWrapper = IntWrapper::create(23);
    }
    Member<IntWrapper> m_intWrapper;
    char m_data[s_length];
};

int LargeObject::s_destructorCalls = 0;

class RefCountedAndGarbageCollected : public RefCountedGarbageCollected<RefCountedAndGarbageCollected> {
    DECLARE_GC_INFO
public:
    static PassRefPtr<RefCountedAndGarbageCollected> create()
    {
        return adoptRef(new RefCountedAndGarbageCollected());
    }

    ~RefCountedAndGarbageCollected()
    {
        ++s_destructorCalls;
    }

    void trace(Visitor*) { }

    static int s_destructorCalls;

private:
    RefCountedAndGarbageCollected()
    {
    }
};

int RefCountedAndGarbageCollected::s_destructorCalls = 0;


class Weak : public Bar {
public:
    static Weak* create(Bar* strong, Bar* weak)
    {
        return new Weak(strong, weak);
    }

    virtual void trace(Visitor* visitor)
    {
        visitor->trace(m_strongBar);
        visitor->registerWeakMembers(this, zapWeakMembers);
    }

    static void zapWeakMembers(Visitor* visitor, void* self)
    {
        reinterpret_cast<Weak*>(self)->zapWeakMembers(visitor);
    }

    bool strongIsThere() { return !!m_strongBar; }
    bool weakIsThere() { return !!m_weakBar; }

private:
    Weak(Bar* strongBar, Bar* weakBar)
        : Bar()
        , m_strongBar(strongBar)
        , m_weakBar(weakBar)
    {
    }

    void zapWeakMembers(Visitor* visitor)
    {
        if (m_weakBar && !visitor->isAlive(m_weakBar))
            m_weakBar = 0;
    }

    Member<Bar> m_strongBar;
    Bar* m_weakBar;
};

class WithWeakMember : public Bar {
public:
    static WithWeakMember* create(Bar* strong, Bar* weak)
    {
        return new WithWeakMember(strong, weak);
    }

    virtual void trace(Visitor* visitor)
    {
        visitor->trace(m_strongBar);
        visitor->trace(m_weakBar);
    }

    bool strongIsThere() { return !!m_strongBar; }
    bool weakIsThere() { return !!m_weakBar; }

private:
    WithWeakMember(Bar* strongBar, Bar* weakBar)
        : Bar()
        , m_strongBar(strongBar)
        , m_weakBar(weakBar)
    {
    }

    Member<Bar> m_strongBar;
    WeakMember<Bar> m_weakBar;
};

TEST(HeapTest, Threading)
{
    Heap::init();
    ThreadedHeapTester::test();
    Heap::shutdown();
}

TEST(HeapTest, Init)
{
    // FIXME: init and shutdown should be called via Blink
    // initialization in the test runner. This test can be removed
    // when that is done.
    Heap::init();
    Heap::shutdown();
}

TEST(HeapTest, SimpleAllocation)
{
    // FIXME: init and shutdown should be called via Blink
    // initialization in the test runner.
    Heap::init();

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
    Heap::init();

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
    Heap::init();

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
    Heap::init();

    {
        // We use TraceCounter for allocating an object on the general heap.
        Persistent<TraceCounter> generalHeapObject = TraceCounter::create();
        Persistent<TestTypedHeapClass> typedHeapObject = TestTypedHeapClass::create();
        EXPECT_NE(pageHeaderAddress(reinterpret_cast<Address>(generalHeapObject.raw())),
            pageHeaderAddress(reinterpret_cast<Address>(typedHeapObject.raw())));
    }

    Heap::shutdown();
}

TEST(HeapTest, NoAllocation)
{
    // FIXME: init and shutdown should be called via Blink
    // initialization in the test runner.
    Heap::init();

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
    Heap::init();

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
    Heap::init();

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
    Heap::init();

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

TEST(HeapTest, HashMapOfMembers)
{
    // FIXME: init and shutdown should be called via Blink
    // initialization in the test runner.
    Heap::init();

    HeapStats initialHeapSize;
    IntWrapper::s_destructorCalls = 0;

    clearOutOldGarbage(&initialHeapSize);
    {
        typedef HashMap<
            Member<IntWrapper>,
            Member<IntWrapper>,
            DefaultHash<Member<IntWrapper> >::Hash,
            HashTraits<Member<IntWrapper> >,
            HashTraits<Member<IntWrapper> >,
            HeapAllocator> HeapObjectIdentityMap;

        HeapObjectIdentityMap* map(new HeapObjectIdentityMap());

        map->clear();
        HeapStats afterSetWasCreated;
        getHeapStats(&afterSetWasCreated);
        EXPECT_TRUE(afterSetWasCreated.totalObjectSpace() > initialHeapSize.totalObjectSpace());

        // FIXME: Activate this when stack scanning works.
        // Heap::collectGarbage(ThreadState::HeapPointersOnStack);
        HeapStats afterGC;
        getHeapStats(&afterGC);
        EXPECT_EQ(afterGC.totalObjectSpace(), afterSetWasCreated.totalObjectSpace());

        IntWrapper* one(IntWrapper::create(1));
        IntWrapper* anotherOne(IntWrapper::create(1));
        map->add(one, one);
        HeapStats afterOneAdd;
        getHeapStats(&afterOneAdd);
        EXPECT_TRUE(afterOneAdd.totalObjectSpace() > afterGC.totalObjectSpace());

        HeapObjectIdentityMap::iterator it(map->begin());
        HeapObjectIdentityMap::iterator it2(map->begin());
        ++it;
        ++it2;

        map->add(anotherOne, one);

        EXPECT_EQ(map->size(), 2u); // Two different wrappings of '1' are distinct.

        // FIXME: Activate this when stack scanning works.
        // Heap::collectGarbage(ThreadState::HeapPointersOnStack);
        EXPECT_TRUE(map->contains(one));
        EXPECT_TRUE(map->contains(anotherOne));

        IntWrapper* gotten(map->get(one));
        EXPECT_EQ(gotten->value(), one->value());
        EXPECT_EQ(gotten, one);

        HeapStats afterGC2;
        getHeapStats(&afterGC2);
        EXPECT_EQ(afterGC2.totalObjectSpace(), afterOneAdd.totalObjectSpace());

        IntWrapper* dozen = 0;

        for (int i = 1; i < 1000; i++) { // 999 iterations.
            IntWrapper* iWrapper(IntWrapper::create(i));
            IntWrapper* iSquared(IntWrapper::create(i * i));
            map->add(iWrapper, iSquared);
            if (i == 12)
                dozen = iWrapper;
        }
        HeapStats afterAdding1000;
        getHeapStats(&afterAdding1000);
        EXPECT_TRUE(afterAdding1000.totalObjectSpace() > afterGC2.totalObjectSpace());

        IntWrapper* gross(map->get(dozen));
        EXPECT_EQ(gross->value(), 144);

        // This should clear out junk created by all the adds.
        // FIXME: Activate this when stack scanning works.
        // Heap::collectGarbage(ThreadState::HeapPointersOnStack);
        HeapStats afterGC3;
        getHeapStats(&afterGC3);
        // FIXME: Reactivate when we sweep.
        // EXPECT_TRUE(afterGC3.totalObjectSpace() < afterAdding1000.totalObjectSpace());
    }

    Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
    // The objects 'one', anotherOne, and the 999 other pairs.
    EXPECT_EQ(IntWrapper::s_destructorCalls, 2000);
    HeapStats afterGC4;
    getHeapStats(&afterGC4);
    EXPECT_EQ(afterGC4.totalObjectSpace(), initialHeapSize.totalObjectSpace());

    Heap::shutdown();
}

TEST(HeapTest, NestedAllocation)
{
    // FIXME: init and shutdown should be called via Blink
    // initialization in the test runner.
    Heap::init();

    HeapStats initialHeapSize;
    clearOutOldGarbage(&initialHeapSize);
    {
        Persistent<ConstructorAllocation> constructorAllocation = ConstructorAllocation::create();
    }
    HeapStats afterFree;
    clearOutOldGarbage(&afterFree);
    EXPECT_TRUE(initialHeapSize == afterFree);

    Heap::shutdown();
}

TEST(HeapTest, LargeObjects)
{
    // FIXME: init and shutdown should be called via Blink
    // initialization in the test runner.
    Heap::init();

    HeapStats initialHeapSize;
    clearOutOldGarbage(&initialHeapSize);
    IntWrapper::s_destructorCalls = 0;
    LargeObject::s_destructorCalls = 0;
    {
        int slack = 8; // LargeObject points to an IntWrapper that is also allocated.
        Persistent<LargeObject> object = LargeObject::create();
        HeapStats afterAllocation;
        clearOutOldGarbage(&afterAllocation);
        {
            object->set(0, 'a');
            EXPECT_EQ('a', object->get(0));
            object->set(object->length() - 1, 'b');
            EXPECT_EQ('b', object->get(object->length() - 1));
            size_t expectedObjectSpace = sizeof(LargeObject) + sizeof(IntWrapper);
            size_t actualObjectSpace =
                afterAllocation.totalObjectSpace() - initialHeapSize.totalObjectSpace();
            CheckWithSlack(expectedObjectSpace, actualObjectSpace, slack);
            // There is probably space for the IntWrapper in a heap page without
            // allocating extra pages. However, the IntWrapper allocation might cause
            // the addition of a heap page.
            size_t largeObjectAllocationSize =
                sizeof(LargeObject) + sizeof(LargeHeapObject<FinalizedHeapObjectHeader>) + sizeof(FinalizedHeapObjectHeader);
            size_t allocatedSpaceLowerBound =
                initialHeapSize.totalAllocatedSpace() + largeObjectAllocationSize;
            size_t allocatedSpaceUpperBound = allocatedSpaceLowerBound + slack + blinkPageSize;
            EXPECT_LE(allocatedSpaceLowerBound, afterAllocation.totalAllocatedSpace());
            EXPECT_LE(afterAllocation.totalAllocatedSpace(), allocatedSpaceUpperBound);
            EXPECT_EQ(0, IntWrapper::s_destructorCalls);
            EXPECT_EQ(0, LargeObject::s_destructorCalls);
            for (int i = 0; i < 10; i++)
                object = LargeObject::create();
        }
        HeapStats oneLargeObject;
        clearOutOldGarbage(&oneLargeObject);
        EXPECT_TRUE(oneLargeObject == afterAllocation);
        EXPECT_EQ(10, IntWrapper::s_destructorCalls);
        EXPECT_EQ(10, LargeObject::s_destructorCalls);
    }
    HeapStats backToInitial;
    clearOutOldGarbage(&backToInitial);
    EXPECT_TRUE(initialHeapSize == backToInitial);
    EXPECT_EQ(11, IntWrapper::s_destructorCalls);
    EXPECT_EQ(11, LargeObject::s_destructorCalls);
    Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);

    Heap::shutdown();
}

TEST(HeapTest, RefCountedGarbageCollected)
{
    // FIXME: init and shutdown should be called via Blink
    // initialization in the test runner.
    Heap::init();

    RefCountedAndGarbageCollected::s_destructorCalls = 0;
    {
        RefPtr<RefCountedAndGarbageCollected> refPtr3;
        {
            Persistent<RefCountedAndGarbageCollected> persistent;
            {
                RefPtr<RefCountedAndGarbageCollected> refPtr1 = RefCountedAndGarbageCollected::create();
                RefPtr<RefCountedAndGarbageCollected> refPtr2 = RefCountedAndGarbageCollected::create();
                Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
                EXPECT_EQ(0, RefCountedAndGarbageCollected::s_destructorCalls);
                persistent = refPtr1.get();
            }
            // Reference count is zero for both objects but one of
            // them is kept alive by a persistent handle.
            Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
            EXPECT_EQ(1, RefCountedAndGarbageCollected::s_destructorCalls);
            refPtr3 = persistent;
        }
        // The persistent handle is gone but the ref count has been
        // increased to 1.
        Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
        EXPECT_EQ(1, RefCountedAndGarbageCollected::s_destructorCalls);
    }
    // Both persistent handle is gone and ref count is zero so the
    // object can be collected.
    Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
    EXPECT_EQ(2, RefCountedAndGarbageCollected::s_destructorCalls);

    Heap::shutdown();
}

TEST(HeapTest, WeakMembers)
{
    // FIXME: init and shutdown should be called via Blink
    // initialization in the test runner.
    Heap::init();

    Bar::s_live = 0;
    {
        Persistent<Bar> h1 = Bar::create();
        Persistent<Weak> h4;
        Persistent<WithWeakMember> h5;
        Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
        ASSERT_EQ(1u, Bar::s_live); // h1 is live.
        {
            // FIXME: Change both persistents below to on stack pointers when
            // supporting conservative scanning.
            Persistent<Bar> h2 = Bar::create();
            Persistent<Bar> h3 = Bar::create();
            h4 = Weak::create(h2, h3);
            h5 = WithWeakMember::create(h2, h3);
            // FIXME: Change to HeapPointersOnStack when changing above to
            // on-stack pointers.
            Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
            EXPECT_EQ(5u, Bar::s_live); // The on-stack pointer keeps h3 alive.
            EXPECT_TRUE(h4->strongIsThere());
            EXPECT_TRUE(h4->weakIsThere());
            EXPECT_TRUE(h5->strongIsThere());
            EXPECT_TRUE(h5->weakIsThere());
        }
        // h3 is collected, weak pointers from h4 and h5 don't keep it alive.
        Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
        EXPECT_EQ(4u, Bar::s_live);
        EXPECT_TRUE(h4->strongIsThere());
        EXPECT_FALSE(h4->weakIsThere()); // h3 is gone from weak pointer.
        EXPECT_TRUE(h5->strongIsThere());
        EXPECT_FALSE(h5->weakIsThere()); // h3 is gone from weak pointer.
        h1.clear(); // Zero out h1.
        Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
        EXPECT_EQ(3u, Bar::s_live); // Only h4, h5 and h2 are left.
        EXPECT_TRUE(h4->strongIsThere()); // h2 is still pointed to from h4.
        EXPECT_TRUE(h5->strongIsThere()); // h2 is still pointed to from h5.
    }
    // h4 and h5 have gone out of scope now and they were keeping h2 alive.
    Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
    EXPECT_EQ(0u, Bar::s_live); // All gone.

    Heap::shutdown();
}

DEFINE_GC_INFO(Bar);
DEFINE_GC_INFO(Baz);
DEFINE_GC_INFO(ClassWithMember);
DEFINE_GC_INFO(ConstructorAllocation);
DEFINE_GC_INFO(HeapAllocatedArray);
DEFINE_GC_INFO(IntWrapper);
DEFINE_GC_INFO(LargeObject);
DEFINE_GC_INFO(RefCountedAndGarbageCollected);
DEFINE_GC_INFO(SimpleFinalizedObject);
DEFINE_GC_INFO(TestTypedHeapClass);
DEFINE_GC_INFO(TraceCounter);

} // namespace
