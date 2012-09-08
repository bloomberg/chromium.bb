// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include <public/WebLayerTreeView.h>

#include "CompositorFakeWebGraphicsContext3D.h"
#include "FakeWebCompositorOutputSurface.h"
#include "WebLayerTreeViewTestCommon.h"
#include <gmock/gmock.h>
#include <public/Platform.h>
#include <public/WebCompositorSupport.h>
#include <public/WebLayer.h>
#include <public/WebLayerTreeViewClient.h>
#include <public/WebThread.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

using namespace WebKit;
using testing::Mock;
using testing::Test;

namespace {

class MockWebLayerTreeViewClientForThreadedTests : public MockWebLayerTreeViewClient {
public:
    virtual void didBeginFrame() OVERRIDE
    {
        WebKit::Platform::current()->currentThread()->exitRunLoop();
        MockWebLayerTreeViewClient::didBeginFrame();
    }
};

class WebLayerTreeViewTestBase : public Test {
protected:
    virtual void initializeCompositor() = 0;
    virtual WebLayerTreeViewClient* client() = 0;

public:
    virtual void SetUp()
    {
        initializeCompositor();
        m_rootLayer = adoptPtr(WebLayer::create());
        ASSERT_TRUE(m_view = adoptPtr(WebLayerTreeView::create(client(), *m_rootLayer, WebLayerTreeView::Settings())));
        m_view->setSurfaceReady();
    }

    virtual void TearDown()
    {
        Mock::VerifyAndClearExpectations(client());

        m_rootLayer.clear();
        m_view.clear();
        WebKit::Platform::current()->compositorSupport()->shutdown();
    }

protected:
    OwnPtr<WebLayer> m_rootLayer;
    OwnPtr<WebLayerTreeView> m_view;
};

class WebLayerTreeViewSingleThreadTest : public WebLayerTreeViewTestBase {
protected:
    void composite()
    {
        m_view->composite();
    }

    virtual void initializeCompositor() OVERRIDE
    {
        WebKit::Platform::current()->compositorSupport()->initialize(0);
    }

    virtual WebLayerTreeViewClient* client() OVERRIDE
    {
        return &m_client;
    }

    MockWebLayerTreeViewClient m_client;
};

class CancelableTaskWrapper : public RefCounted<CancelableTaskWrapper> {
    class Task : public WebThread::Task {
    public:
        Task(CancelableTaskWrapper* cancelableTask)
            : m_cancelableTask(cancelableTask)
        {
        }

    private:
        virtual void run() OVERRIDE
        {
            m_cancelableTask->runIfNotCanceled();
        }

        RefPtr<CancelableTaskWrapper> m_cancelableTask;
    };

public:
    CancelableTaskWrapper(PassOwnPtr<WebThread::Task> task)
        : m_task(task)
    {
    }

    void cancel()
    {
        m_task.clear();
    }

    WebThread::Task* createTask()
    {
        ASSERT(m_task);
        return new Task(this);
    }

    void runIfNotCanceled()
    {
        if (!m_task)
            return;
        m_task->run();
        m_task.clear();
    }

private:
    OwnPtr<WebThread::Task> m_task;
};

class WebLayerTreeViewThreadedTest : public WebLayerTreeViewTestBase {
protected:
    class TimeoutTask : public WebThread::Task {
        virtual void run() OVERRIDE
        {
            WebKit::Platform::current()->currentThread()->exitRunLoop();
        }
    };

    void composite()
    {
        m_view->setNeedsRedraw();
        RefPtr<CancelableTaskWrapper> timeoutTask = adoptRef(new CancelableTaskWrapper(adoptPtr(new TimeoutTask())));
        WebKit::Platform::current()->currentThread()->postDelayedTask(timeoutTask->createTask(), 5000);
        WebKit::Platform::current()->currentThread()->enterRunLoop();
        timeoutTask->cancel();
        m_view->finishAllRendering();
    }

    virtual void initializeCompositor() OVERRIDE
    {
        m_webThread = adoptPtr(WebKit::Platform::current()->createThread("WebLayerTreeViewTest"));
        WebKit::Platform::current()->compositorSupport()->initialize(m_webThread.get());
    }

    virtual WebLayerTreeViewClient* client() OVERRIDE
    {
        return &m_client;
    }

    MockWebLayerTreeViewClientForThreadedTests m_client;
    OwnPtr<WebThread> m_webThread;
};

TEST_F(WebLayerTreeViewSingleThreadTest, InstrumentationCallbacks)
{
    ::testing::InSequence dummy;

    EXPECT_CALL(m_client, willCommit());
    EXPECT_CALL(m_client, didCommit());
    EXPECT_CALL(m_client, didBeginFrame());

    composite();
}

TEST_F(WebLayerTreeViewThreadedTest, InstrumentationCallbacks)
{
    ::testing::InSequence dummy;

    EXPECT_CALL(m_client, willBeginFrame());
    EXPECT_CALL(m_client, willCommit());
    EXPECT_CALL(m_client, didCommit());
    EXPECT_CALL(m_client, didBeginFrame());

    composite();
}

} // namespace
