// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/canvas/CanvasAsyncBlobCreator.h"

#include "core/html/ImageData.h"
#include "platform/Task.h"
#include "platform/testing/UnitTestHelpers.h"
#include "public/platform/Platform.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/Functional.h"

namespace blink {

typedef CanvasAsyncBlobCreator::IdleTaskStatus IdleTaskStatus;

class MockCanvasAsyncBlobCreator : public CanvasAsyncBlobCreator {
public:
    MockCanvasAsyncBlobCreator(PassRefPtr<DOMUint8ClampedArray> data, const IntSize& size)
        : CanvasAsyncBlobCreator(data, "image/png", size, nullptr)
    {
    }

    CanvasAsyncBlobCreator::IdleTaskStatus idleTaskStatus()
    {
        return m_idleTaskStatus;
    }

    MOCK_METHOD0(signalTaskSwitchInStartTimeoutEventForTesting, void());
    MOCK_METHOD0(signalTaskSwitchInCompleteTimeoutEventForTesting, void());

protected:
    void createBlobAndCall() override { };
    void createNullptrAndCall() override { };
    void clearAlternativeSelfReference() override;
    void postDelayedTaskToMainThread(const WebTraceLocation&, Task*, double delayMs) override;
};

void MockCanvasAsyncBlobCreator::clearAlternativeSelfReference()
{
    testing::exitRunLoop();
}

void MockCanvasAsyncBlobCreator::postDelayedTaskToMainThread(const WebTraceLocation& location, Task* task, double delayMs)
{
    Platform::current()->mainThread()->taskRunner()->postTask(location, task);
}

//==============================================================================

class MockCanvasAsyncBlobCreatorWithoutStart : public MockCanvasAsyncBlobCreator {
public:
    MockCanvasAsyncBlobCreatorWithoutStart(PassRefPtr<DOMUint8ClampedArray> data, const IntSize& size)
        : MockCanvasAsyncBlobCreator(data, size)
    {
    }

protected:
    void scheduleInitiatePngEncoding() override
    {
        // Deliberately make scheduleInitiatePngEncoding do nothing so that idle task never starts
        this->clearSelfReference();
    }
};

//==============================================================================

class MockCanvasAsyncBlobCreatorWithoutComplete : public MockCanvasAsyncBlobCreator {
public:
    MockCanvasAsyncBlobCreatorWithoutComplete(PassRefPtr<DOMUint8ClampedArray> data, const IntSize& size)
        : MockCanvasAsyncBlobCreator(data, size)
    {
    }

protected:
    void scheduleInitiatePngEncoding() override
    {
        Platform::current()->mainThread()->taskRunner()->postTask(BLINK_FROM_HERE,
        bind(&MockCanvasAsyncBlobCreatorWithoutComplete::initiatePngEncoding, this, std::numeric_limits<double>::max()));
    }

    void idleEncodeRowsPng(double deadlineSeconds) override
    {
        // Deliberately make idleEncodeRowsPng do nothing so that idle task never completes
        this->clearSelfReference();
    }
};

//==============================================================================

class CanvasAsyncBlobCreatorTest : public ::testing::Test {
public:
    void prepareMockCanvasAsyncBlobCreatorWithoutStart();
    void prepareMockCanvasAsyncBlobCreatorWithoutComplete();
    void prepareMockCanvasAsyncBlobCreatorFail();

protected:
    CanvasAsyncBlobCreatorTest();
    MockCanvasAsyncBlobCreator* asyncBlobCreator() { return m_asyncBlobCreator.get(); }
    void TearDown() override;

private:
    RefPtr<MockCanvasAsyncBlobCreator> m_asyncBlobCreator;
};

CanvasAsyncBlobCreatorTest::CanvasAsyncBlobCreatorTest()
{
}

void CanvasAsyncBlobCreatorTest::prepareMockCanvasAsyncBlobCreatorWithoutStart()
{
    IntSize testSize(20, 20);
    ImageData* imageData = ImageData::create(testSize);
    RefPtr<DOMUint8ClampedArray> imageDataRef(imageData->data());

    m_asyncBlobCreator = adoptRef(new MockCanvasAsyncBlobCreatorWithoutStart(imageDataRef.release(), testSize));
}

void CanvasAsyncBlobCreatorTest::prepareMockCanvasAsyncBlobCreatorWithoutComplete()
{
    IntSize testSize(20, 20);
    ImageData* imageData = ImageData::create(testSize);
    RefPtr<DOMUint8ClampedArray> imageDataRef(imageData->data());

    m_asyncBlobCreator = adoptRef(new MockCanvasAsyncBlobCreatorWithoutComplete(imageDataRef.release(), testSize));
}

void CanvasAsyncBlobCreatorTest::prepareMockCanvasAsyncBlobCreatorFail()
{
    IntSize testSize(0, 0);
    ImageData* imageData = ImageData::create(testSize);
    RefPtr<DOMUint8ClampedArray> imageDataRef(imageData->data());

    // We reuse the class MockCanvasAsyncBlobCreatorWithoutComplete because this
    // test case is expected to fail at initialization step before completion.
    m_asyncBlobCreator = adoptRef(new MockCanvasAsyncBlobCreatorWithoutComplete(imageDataRef.release(), testSize));
}

void CanvasAsyncBlobCreatorTest::TearDown()
{
    ASSERT(!!m_asyncBlobCreator->m_alternativeSelfRef);
    m_asyncBlobCreator->m_alternativeSelfRef.clear();
    m_asyncBlobCreator = nullptr;
}

//==============================================================================

TEST_F(CanvasAsyncBlobCreatorTest, IdleTaskNotStartedWhenStartTimeoutEventHappens)
{
    // This test mocks the scenario when idle task is not started when the
    // StartTimeoutEvent is inspecting the idle task status.
    // The whole image encoding process (including initialization)  will then
    // become carried out in the alternative code path instead.
    this->prepareMockCanvasAsyncBlobCreatorWithoutStart();
    EXPECT_CALL(*(asyncBlobCreator()), signalTaskSwitchInStartTimeoutEventForTesting());

    this->asyncBlobCreator()->scheduleAsyncBlobCreation(true);
    testing::enterRunLoop();

    ::testing::Mock::VerifyAndClearExpectations(asyncBlobCreator());
    EXPECT_EQ(IdleTaskStatus::IdleTaskSwitchedToMainThreadTask, this->asyncBlobCreator()->idleTaskStatus());
}

TEST_F(CanvasAsyncBlobCreatorTest, IdleTaskNotCompletedWhenCompleteTimeoutEventHappens)
{
    // This test mocks the scenario when idle task is not completed when the
    // CompleteTimeoutEvent is inspecting the idle task status.
    // The remaining image encoding process (excluding initialization)  will
    // then become carried out in the alternative code path instead.
    this->prepareMockCanvasAsyncBlobCreatorWithoutComplete();
    EXPECT_CALL(*(asyncBlobCreator()), signalTaskSwitchInCompleteTimeoutEventForTesting());

    this->asyncBlobCreator()->scheduleAsyncBlobCreation(true);
    testing::enterRunLoop();

    ::testing::Mock::VerifyAndClearExpectations(asyncBlobCreator());
    EXPECT_EQ(IdleTaskStatus::IdleTaskSwitchedToMainThreadTask, this->asyncBlobCreator()->idleTaskStatus());
}

TEST_F(CanvasAsyncBlobCreatorTest, IdleTaskFailedWhenStartTimeoutEventHappens)
{
    // This test mocks the scenario when idle task is not failed during when
    // either the StartTimeoutEvent or the CompleteTimeoutEvent is inspecting
    // the idle task status.
    this->prepareMockCanvasAsyncBlobCreatorFail();

    this->asyncBlobCreator()->scheduleAsyncBlobCreation(true);
    testing::enterRunLoop();

    EXPECT_EQ(IdleTaskStatus::IdleTaskFailed, this->asyncBlobCreator()->idleTaskStatus());
}

}
