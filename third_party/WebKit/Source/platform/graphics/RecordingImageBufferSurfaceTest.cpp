// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "platform/graphics/RecordingImageBufferSurface.h"

#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/ImageBuffer.h"
#include "public/platform/Platform.h"
#include "public/platform/WebThread.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/RefPtr.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace blink;
using testing::Test;

class RecordingImageBufferSurfaceTest : public Test {
protected:
    RecordingImageBufferSurfaceTest()
    {
        OwnPtr<RecordingImageBufferSurface> testSurface = adoptPtr(new RecordingImageBufferSurface(IntSize(10, 10)));
        m_testSurface = testSurface.get();
        // We create an ImageBuffer in order for the testSurface to be
        // properly initialized with a GraphicsContext
        m_imageBuffer = ImageBuffer::create(testSurface.release());
    }

public:
    void testEmptyPicture()
    {
        m_testSurface->initializeCurrentFrame();
        RefPtr<SkPicture> picture = m_testSurface->getPicture();
        EXPECT_TRUE((bool)picture.get());
        expectDisplayListEnabled(true);
    }

    void testNoFallbackWithClear()
    {
        m_testSurface->initializeCurrentFrame();
        m_testSurface->didClearCanvas();
        m_testSurface->getPicture();
        expectDisplayListEnabled(true);
    }

    void testNonAnimatedCanvasUpdate()
    {
        m_testSurface->initializeCurrentFrame();
        // acquire picture twice to simulate a static canvas: nothing drawn between updates
        m_testSurface->getPicture();
        m_testSurface->getPicture();
        expectDisplayListEnabled(true);
    }

    void testAnimatedWithoutClear()
    {
        m_testSurface->initializeCurrentFrame();
        m_testSurface->getPicture();
        m_testSurface->didDraw();
        expectDisplayListEnabled(true);
        // This will trigger fallback
        m_testSurface->getPicture();
        expectDisplayListEnabled(false);
    }

    void testFrameFinalizedByTaskObserver1()
    {
        m_testSurface->initializeCurrentFrame();
        expectDisplayListEnabled(true);
        m_testSurface->getPicture();
        expectDisplayListEnabled(true);
        m_testSurface->didDraw();
        expectDisplayListEnabled(true);
        // Display list will be disabled only after exiting the runLoop
    }
    void testFrameFinalizedByTaskObserver2()
    {
        expectDisplayListEnabled(false);
        m_testSurface->getPicture();
        expectDisplayListEnabled(false);
        m_testSurface->didDraw();
        expectDisplayListEnabled(false);
    }

    void testAnimatedWithClear()
    {
        m_testSurface->initializeCurrentFrame();
        m_testSurface->getPicture();
        m_testSurface->didClearCanvas();
        m_testSurface->didDraw();
        m_testSurface->getPicture();
        expectDisplayListEnabled(true);
        // clear after use
        m_testSurface->didDraw();
        m_testSurface->didClearCanvas();
        m_testSurface->getPicture();
        expectDisplayListEnabled(true);
    }

    void testClearRect()
    {
        m_testSurface->initializeCurrentFrame();
        m_testSurface->getPicture();
        m_imageBuffer->context()->clearRect(FloatRect(FloatPoint(0, 0), FloatSize(m_testSurface->size())));
        m_testSurface->didDraw();
        m_testSurface->getPicture();
        expectDisplayListEnabled(true);
    }

    void expectDisplayListEnabled(bool displayListEnabled)
    {
        EXPECT_EQ(displayListEnabled, (bool)m_testSurface->m_currentFrame.get());
        EXPECT_EQ(!displayListEnabled, (bool)m_testSurface->m_rasterCanvas.get());
    }

private:
    RecordingImageBufferSurface* m_testSurface;
    OwnPtr<ImageBuffer> m_imageBuffer;
};

namespace {

// The following test helper class installs a mock platform that provides a mock WebThread
// for the current thread. The Mock thread is capable of queuing a single non-delayed task
// and registering a single task observer. The run loop exits immediately after running
// the single task.
class AutoInstallCurrentThreadPlatformMock {
public:
    AutoInstallCurrentThreadPlatformMock()
    {
        m_oldPlatform = blink::Platform::current();
        blink::Platform::initialize(&m_mockPlatform);
    }

    ~AutoInstallCurrentThreadPlatformMock()
    {
        blink::Platform::initialize(m_oldPlatform);
    }

private:
    class CurrentThreadMock : public WebThread {
    public:
        CurrentThreadMock() : m_taskObserver(0), m_task(0) { }

        virtual ~CurrentThreadMock()
        {
            EXPECT_EQ((Task*)0, m_task);
        }

        virtual void postTask(Task* task)
        {
            EXPECT_EQ((Task*)0, m_task);
            m_task = task;
        }

        virtual void postDelayedTask(Task*, long long delayMs) OVERRIDE { ASSERT_NOT_REACHED(); };

        virtual bool isCurrentThread() const OVERRIDE { return true; }

        virtual void addTaskObserver(TaskObserver* taskObserver) OVERRIDE
        {
            EXPECT_EQ((TaskObserver*)0, m_taskObserver);
            m_taskObserver = taskObserver;
        }

        virtual void removeTaskObserver(TaskObserver* taskObserver) OVERRIDE
        {
            EXPECT_EQ(m_taskObserver, taskObserver);
            m_taskObserver = 0;
        }

        virtual void enterRunLoop() OVERRIDE
        {
            if (m_taskObserver)
                m_taskObserver->willProcessTask();
            if (m_task) {
                m_task->run();
                delete m_task;
                m_task = 0;
            }
            if (m_taskObserver)
                m_taskObserver->didProcessTask();
        }

        virtual void exitRunLoop() OVERRIDE { ASSERT_NOT_REACHED(); }

    private:
        TaskObserver* m_taskObserver;
        Task* m_task;
    };

    class CurrentThreadPlatformMock : public blink::Platform {
    public:
        CurrentThreadPlatformMock() { }
        virtual void cryptographicallyRandomValues(unsigned char* buffer, size_t length) { ASSERT_NOT_REACHED(); }
        virtual WebThread* currentThread() OVERRIDE { return &m_currentThread; }
    private:
        CurrentThreadMock m_currentThread;
    };

    CurrentThreadPlatformMock m_mockPlatform;
    blink::Platform* m_oldPlatform;
};


#define DEFINE_TEST_TASK_WRAPPER_CLASS(TEST_METHOD)                                               \
class TestWrapperTask_ ## TEST_METHOD : public blink::WebThread::Task {                           \
    public:                                                                                       \
        TestWrapperTask_ ## TEST_METHOD(RecordingImageBufferSurfaceTest* test) : m_test(test) { } \
        virtual void run() OVERRIDE { m_test->TEST_METHOD(); }                                    \
    private:                                                                                      \
        RecordingImageBufferSurfaceTest* m_test;                                                  \
};

#define CALL_TEST_TASK_WRAPPER(TEST_METHOD)                                                               \
    {                                                                                                     \
        AutoInstallCurrentThreadPlatformMock ctpm;                                                        \
        blink::Platform::current()->currentThread()->postTask(new TestWrapperTask_ ## TEST_METHOD(this)); \
        blink::Platform::current()->currentThread()->enterRunLoop();                                      \
    }

TEST_F(RecordingImageBufferSurfaceTest, testEmptyPicture)
{
    testEmptyPicture();
}

TEST_F(RecordingImageBufferSurfaceTest, testNoFallbackWithClear)
{
    testNoFallbackWithClear();
}

TEST_F(RecordingImageBufferSurfaceTest, testNonAnimatedCanvasUpdate)
{
    testNonAnimatedCanvasUpdate();
}

DEFINE_TEST_TASK_WRAPPER_CLASS(testAnimatedWithoutClear)
TEST_F(RecordingImageBufferSurfaceTest, testAnimatedWithoutClear)
{
    CALL_TEST_TASK_WRAPPER(testAnimatedWithoutClear)
    expectDisplayListEnabled(false);
}

DEFINE_TEST_TASK_WRAPPER_CLASS(testFrameFinalizedByTaskObserver1)
DEFINE_TEST_TASK_WRAPPER_CLASS(testFrameFinalizedByTaskObserver2)
TEST_F(RecordingImageBufferSurfaceTest, testFrameFinalizedByTaskObserver)
{
    CALL_TEST_TASK_WRAPPER(testFrameFinalizedByTaskObserver1)
    expectDisplayListEnabled(false);
    CALL_TEST_TASK_WRAPPER(testFrameFinalizedByTaskObserver2)
    expectDisplayListEnabled(false);
}

DEFINE_TEST_TASK_WRAPPER_CLASS(testAnimatedWithClear)
TEST_F(RecordingImageBufferSurfaceTest, testAnimatedWithClear)
{
    CALL_TEST_TASK_WRAPPER(testAnimatedWithClear)
    expectDisplayListEnabled(true);
}

DEFINE_TEST_TASK_WRAPPER_CLASS(testClearRect)
TEST_F(RecordingImageBufferSurfaceTest, testClearRect)
{
    CALL_TEST_TASK_WRAPPER(testClearRect);
    expectDisplayListEnabled(true);
}

} // namespace
