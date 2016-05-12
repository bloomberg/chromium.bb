// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/CancellableTaskFactory.h"

#include "platform/heap/Handle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

class TestCancellableTaskFactory final : public CancellableTaskFactory {
public:
    explicit TestCancellableTaskFactory(std::unique_ptr<SameThreadClosure> closure)
        : CancellableTaskFactory(std::move(closure))
    {
    }
};

} // namespace

using CancellableTaskFactoryTest = testing::Test;

TEST_F(CancellableTaskFactoryTest, IsPending_TaskNotCreated)
{
    TestCancellableTaskFactory factory(nullptr);

    EXPECT_FALSE(factory.isPending());
}

TEST_F(CancellableTaskFactoryTest, IsPending_TaskCreated)
{
    TestCancellableTaskFactory factory(nullptr);
    OwnPtr<WebTaskRunner::Task> task = adoptPtr(factory.cancelAndCreate());

    EXPECT_TRUE(factory.isPending());
}

void EmptyFn()
{
}

TEST_F(CancellableTaskFactoryTest, IsPending_TaskCreatedAndRun)
{
    TestCancellableTaskFactory factory(WTF::bind(&EmptyFn));
    {
        OwnPtr<WebTaskRunner::Task> task = adoptPtr(factory.cancelAndCreate());
        task->run();
    }

    EXPECT_FALSE(factory.isPending());
}

TEST_F(CancellableTaskFactoryTest, IsPending_TaskCreatedAndDestroyed)
{
    TestCancellableTaskFactory factory(nullptr);
    delete factory.cancelAndCreate();

    EXPECT_FALSE(factory.isPending());
}

TEST_F(CancellableTaskFactoryTest, IsPending_TaskCreatedAndCancelled)
{
    TestCancellableTaskFactory factory(nullptr);
    OwnPtr<WebTaskRunner::Task> task = adoptPtr(factory.cancelAndCreate());
    factory.cancel();

    EXPECT_FALSE(factory.isPending());
}

class TestClass {
public:
    OwnPtr<CancellableTaskFactory> m_factory;

    TestClass()
        : m_factory(CancellableTaskFactory::create(this, &TestClass::TestFn))
    {
    }

    void TestFn()
    {
        EXPECT_FALSE(m_factory->isPending());
    }
};

TEST_F(CancellableTaskFactoryTest, IsPending_InCallback)
{
    TestClass testClass;
    OwnPtr<WebTaskRunner::Task> task = adoptPtr(testClass.m_factory->cancelAndCreate());
    task->run();
}

void AddOne(int* ptr)
{
    *ptr += 1;
}

TEST_F(CancellableTaskFactoryTest, Run_ClosureIsExecuted)
{
    int executionCount = 0;
    TestCancellableTaskFactory factory(WTF::bind(&AddOne, &executionCount));
    OwnPtr<WebTaskRunner::Task> task = adoptPtr(factory.cancelAndCreate());
    task->run();

    EXPECT_EQ(1, executionCount);
}

TEST_F(CancellableTaskFactoryTest, Run_ClosureIsExecutedOnlyOnce)
{
    int executionCount = 0;
    TestCancellableTaskFactory factory(WTF::bind(&AddOne, &executionCount));
    OwnPtr<WebTaskRunner::Task> task = adoptPtr(factory.cancelAndCreate());
    task->run();
    task->run();
    task->run();
    task->run();

    EXPECT_EQ(1, executionCount);
}

TEST_F(CancellableTaskFactoryTest, Run_FactoryDestructionPreventsExecution)
{
    int executionCount = 0;
    OwnPtr<WebTaskRunner::Task> task;
    {
        TestCancellableTaskFactory factory(WTF::bind(&AddOne, &executionCount));
        task = adoptPtr(factory.cancelAndCreate());
    }
    task->run();

    EXPECT_EQ(0, executionCount);
}

TEST_F(CancellableTaskFactoryTest, Run_TasksInSequence)
{
    int executionCount = 0;
    TestCancellableTaskFactory factory(WTF::bind(&AddOne, &executionCount));

    OwnPtr<WebTaskRunner::Task> taskA = adoptPtr(factory.cancelAndCreate());
    taskA->run();
    EXPECT_EQ(1, executionCount);

    OwnPtr<WebTaskRunner::Task> taskB = adoptPtr(factory.cancelAndCreate());
    taskB->run();
    EXPECT_EQ(2, executionCount);

    OwnPtr<WebTaskRunner::Task> taskC = adoptPtr(factory.cancelAndCreate());
    taskC->run();
    EXPECT_EQ(3, executionCount);
}

TEST_F(CancellableTaskFactoryTest, Cancel)
{
    int executionCount = 0;
    TestCancellableTaskFactory factory(WTF::bind(&AddOne, &executionCount));
    OwnPtr<WebTaskRunner::Task> task = adoptPtr(factory.cancelAndCreate());
    factory.cancel();
    task->run();

    EXPECT_EQ(0, executionCount);
}

TEST_F(CancellableTaskFactoryTest, CreatingANewTaskCancelsPreviousOnes)
{
    int executionCount = 0;
    TestCancellableTaskFactory factory(WTF::bind(&AddOne, &executionCount));

    OwnPtr<WebTaskRunner::Task> taskA = adoptPtr(factory.cancelAndCreate());
    OwnPtr<WebTaskRunner::Task> taskB = adoptPtr(factory.cancelAndCreate());

    taskA->run();
    EXPECT_EQ(0, executionCount);

    taskB->run();
    EXPECT_EQ(1, executionCount);
}

namespace {

class GCObject final : public GarbageCollectedFinalized<GCObject> {
public:
    GCObject()
        : m_factory(CancellableTaskFactory::create(this, &GCObject::run))
    {
    }

    ~GCObject()
    {
        s_destructed++;
    }

    void run()
    {
        s_invoked++;
    }

    DEFINE_INLINE_TRACE() { }

    static int s_destructed;
    static int s_invoked;

    OwnPtr<CancellableTaskFactory> m_factory;
};

int GCObject::s_destructed = 0;
int GCObject::s_invoked = 0;

} // namespace

TEST(CancellableTaskFactoryTest, GarbageCollectedWeak)
{
    GCObject* object = new GCObject();
    OwnPtr<WebTaskRunner::Task> task = adoptPtr(object->m_factory->cancelAndCreate());
    object = nullptr;
    ThreadHeap::collectAllGarbage();
    task->run();
    // The owning object will have been GCed and the task will have
    // lost its weak reference. Verify that it wasn't invoked.
    EXPECT_EQ(0, GCObject::s_invoked);

    // ..and just to make sure |object| was indeed destructed.
    EXPECT_EQ(1, GCObject::s_destructed);
}

} // namespace blink
