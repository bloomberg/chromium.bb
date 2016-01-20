// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/canvas/CanvasAsyncBlobCreator.h"

#include "core/html/ImageData.h"
#include "platform/ThreadSafeFunctional.h"
#include "platform/threading/BackgroundTaskRunner.h"
#include "public/platform/Platform.h"
#include "public/platform/WebWaitableEvent.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class MockCanvasAsyncBlobCreator;

class FakeContextObserver {
public:
    FakeContextObserver(MockCanvasAsyncBlobCreator* asyncBlobCreator)
        : m_asyncBlobCreator(asyncBlobCreator)
    {
    }

    void contextDestroyed();

private:
    MockCanvasAsyncBlobCreator* m_asyncBlobCreator;
};

//============================================================================

class MockCanvasAsyncBlobCreator : public CanvasAsyncBlobCreator {
public:
    MockCanvasAsyncBlobCreator(PassRefPtr<DOMUint8ClampedArray> data, const String& mimeType, const IntSize& size)
        : CanvasAsyncBlobCreator(data, mimeType, size, nullptr)
    {
    }

    void setFakeContextObserver(FakeContextObserver* contextObserver)
    {
        m_contextObserver = contextObserver;
    }

    bool isCancelled() { return m_cancelled; }
    void onContextDestroyed()
    {
        m_cancelled = true;
    }
    MOCK_METHOD0(scheduleClearSelfRefOnMainThread, void());
    MOCK_METHOD0(scheduleCreateNullptrAndCallOnMainThread, void());
    MOCK_METHOD0(scheduleCreateBlobAndCallOnMainThread, void());

private:
    FakeContextObserver* m_contextObserver;
};

//============================================================================

class CanvasAsyncBlobCreatorTest : public ::testing::Test {
public:
    void prepareMockCanvasAsyncBlobCreator(int width, int height);
    bool initializeEncodeImage() { return m_asyncBlobCreator->initializeEncodeImageOnEncoderThread(); }
    void startEncodeImage() { m_asyncBlobCreator->progressiveEncodeImageOnEncoderThread(); }

protected:
    CanvasAsyncBlobCreatorTest();
    void TearDown() override;
    MockCanvasAsyncBlobCreator* asyncBlobCreator();
    FakeContextObserver* contextObserver();

private:
    RefPtr<MockCanvasAsyncBlobCreator> m_asyncBlobCreator;
    FakeContextObserver* m_contextObserver;
};

//============================================================================

CanvasAsyncBlobCreatorTest::CanvasAsyncBlobCreatorTest()
{
}

void CanvasAsyncBlobCreatorTest::prepareMockCanvasAsyncBlobCreator(int width, int height)
{
    IntSize testSize(width, height);
    ImageData* imageData = ImageData::create(testSize);
    RefPtr<DOMUint8ClampedArray> imageDataRef(imageData->data());

    m_asyncBlobCreator = adoptRef(new MockCanvasAsyncBlobCreator(imageDataRef.release(), "image/png", testSize));
    m_contextObserver = new FakeContextObserver(m_asyncBlobCreator.get());
    m_asyncBlobCreator->setFakeContextObserver(m_contextObserver);
}

void CanvasAsyncBlobCreatorTest::TearDown()
{
    delete m_contextObserver;
    m_asyncBlobCreator->m_selfRef.clear();
}

void FakeContextObserver::contextDestroyed()
{
    m_asyncBlobCreator->onContextDestroyed();
    m_asyncBlobCreator = nullptr;
}

void ImageEncodingTask(CanvasAsyncBlobCreatorTest* testHost, WebWaitableEvent* startEvent, WebWaitableEvent* doneEvent)
{
    bool initializationSuccess = testHost->initializeEncodeImage();
    startEvent->signal();
    if (initializationSuccess)
        testHost->startEncodeImage();
    doneEvent->signal();
}

void ImageEncodingTaskWithoutStartSignal(CanvasAsyncBlobCreatorTest* testHost, WebWaitableEvent* doneEvent)
{
    bool initializationSuccess = testHost->initializeEncodeImage();
    if (initializationSuccess)
        testHost->startEncodeImage();
    doneEvent->signal();
}

MockCanvasAsyncBlobCreator* CanvasAsyncBlobCreatorTest::asyncBlobCreator()
{
    return m_asyncBlobCreator.get();
}

FakeContextObserver* CanvasAsyncBlobCreatorTest::contextObserver()
{
    return m_contextObserver;
}

TEST_F(CanvasAsyncBlobCreatorTest, CancelImageEncodingWhenContextTornDown)
{
    this->prepareMockCanvasAsyncBlobCreator(4000, 4000);

    OwnPtr<WebWaitableEvent> startEvent = adoptPtr(Platform::current()->createWaitableEvent());
    OwnPtr<WebWaitableEvent> doneEvent = adoptPtr(Platform::current()->createWaitableEvent());

    EXPECT_CALL(*(asyncBlobCreator()), scheduleClearSelfRefOnMainThread());

    BackgroundTaskRunner::postOnBackgroundThread(BLINK_FROM_HERE, threadSafeBind(&ImageEncodingTask, AllowCrossThreadAccess(this), AllowCrossThreadAccess(startEvent.get()), AllowCrossThreadAccess(doneEvent.get())), BackgroundTaskRunner::TaskSizeLongRunningTask);
    startEvent->wait();
    contextObserver()->contextDestroyed();
    doneEvent->wait();

    ::testing::Mock::VerifyAndClearExpectations(asyncBlobCreator());
    EXPECT_TRUE(asyncBlobCreator()->isCancelled());
}

TEST_F(CanvasAsyncBlobCreatorTest, CompleteImageEncodingWithoutContextTornDown)
{
    this->prepareMockCanvasAsyncBlobCreator(50, 50);

    OwnPtr<WebWaitableEvent> doneEvent = adoptPtr(Platform::current()->createWaitableEvent());

    EXPECT_CALL(*(asyncBlobCreator()), scheduleCreateBlobAndCallOnMainThread());

    BackgroundTaskRunner::postOnBackgroundThread(BLINK_FROM_HERE, threadSafeBind(&ImageEncodingTaskWithoutStartSignal, AllowCrossThreadAccess(this), AllowCrossThreadAccess(doneEvent.get())), BackgroundTaskRunner::TaskSizeShortRunningTask);
    doneEvent->wait();

    ::testing::Mock::VerifyAndClearExpectations(asyncBlobCreator());
    EXPECT_FALSE(asyncBlobCreator()->isCancelled());
}

TEST_F(CanvasAsyncBlobCreatorTest, FailedImageEncodingOnEmptyImageData)
{
    this->prepareMockCanvasAsyncBlobCreator(0, 0);

    OwnPtr<WebWaitableEvent> doneEvent = adoptPtr(Platform::current()->createWaitableEvent());

    EXPECT_CALL(*(asyncBlobCreator()), scheduleCreateNullptrAndCallOnMainThread());

    BackgroundTaskRunner::postOnBackgroundThread(BLINK_FROM_HERE, threadSafeBind(&ImageEncodingTaskWithoutStartSignal, AllowCrossThreadAccess(this), AllowCrossThreadAccess(doneEvent.get())), BackgroundTaskRunner::TaskSizeShortRunningTask);
    doneEvent->wait();

    ::testing::Mock::VerifyAndClearExpectations(asyncBlobCreator());
    EXPECT_FALSE(asyncBlobCreator()->isCancelled());
}

} // namespace blink
