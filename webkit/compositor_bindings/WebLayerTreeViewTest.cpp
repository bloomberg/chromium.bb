// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "WebLayerImpl.h"
#include "WebLayerTreeViewImpl.h"
#include "WebLayerTreeViewTestCommon.h"
#include "base/memory/ref_counted.h"
#include "cc/test/compositor_fake_web_graphics_context_3d.h"
#include "cc/test/fake_web_compositor_output_surface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/WebKit/Source/Platform/chromium/public/Platform.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebCompositorSupport.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebLayer.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebLayerTreeView.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebLayerTreeViewClient.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebThread.h"

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
        m_rootLayer.reset(new WebLayerImpl);
        m_view.reset(new WebLayerTreeViewImpl(client()));
        ASSERT_TRUE(m_view->initialize(WebLayerTreeView::Settings()));
        m_view->setRootLayer(*m_rootLayer);
        m_view->setSurfaceReady();
    }

    virtual void TearDown()
    {
        Mock::VerifyAndClearExpectations(client());

        m_rootLayer.reset();
        m_view.reset();
        WebKit::Platform::current()->compositorSupport()->shutdown();
    }

protected:
    scoped_ptr<WebLayer> m_rootLayer;
    scoped_ptr<WebLayerTreeViewImpl> m_view;
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

class CancelableTaskWrapper : public base::RefCounted<CancelableTaskWrapper> {
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

        scoped_refptr<CancelableTaskWrapper> m_cancelableTask;
    };

public:
    CancelableTaskWrapper(scoped_ptr<WebThread::Task> task)
        : m_task(task.Pass())
    {
    }

    void cancel()
    {
        m_task.reset();
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
        m_task.reset();
    }

private:
    friend class base::RefCounted<CancelableTaskWrapper>;
    ~CancelableTaskWrapper() { }

    scoped_ptr<WebThread::Task> m_task;
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
        scoped_refptr<CancelableTaskWrapper> timeoutTask(new CancelableTaskWrapper(scoped_ptr<WebThread::Task>(new TimeoutTask())));
        WebKit::Platform::current()->currentThread()->postDelayedTask(timeoutTask->createTask(), 5000);
        WebKit::Platform::current()->currentThread()->enterRunLoop();
        timeoutTask->cancel();
        m_view->finishAllRendering();
    }

    virtual void initializeCompositor() OVERRIDE
    {
        m_webThread.reset(WebKit::Platform::current()->createThread("WebLayerTreeViewTest"));
        WebKit::Platform::current()->compositorSupport()->initialize(m_webThread.get());
    }

    virtual WebLayerTreeViewClient* client() OVERRIDE
    {
        return &m_client;
    }

    MockWebLayerTreeViewClientForThreadedTests m_client;
    scoped_ptr<WebThread> m_webThread;
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
