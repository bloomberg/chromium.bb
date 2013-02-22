// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/cancelable_callback.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread.h"
#include "cc/proxy.h"
#include "cc/thread_impl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebLayer.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebLayerTreeViewClient.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebLayerTreeView.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebThread.h"
#include "webkit/compositor_bindings/test/web_layer_tree_view_test_common.h"
#include "webkit/compositor_bindings/web_layer_impl.h"
#include "webkit/compositor_bindings/web_layer_tree_view_impl_for_testing.h"

using namespace WebKit;
using testing::Mock;
using testing::Test;

namespace {

class MockWebLayerTreeViewClientForThreadedTests : public MockWebLayerTreeViewClient {
public:
    virtual void didBeginFrame() OVERRIDE
    {
        MessageLoop::current()->Quit();
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
        m_view.reset(new WebLayerTreeViewImplForTesting(
            WebLayerTreeViewImplForTesting::FAKE_CONTEXT, client()));
        scoped_ptr<cc::Thread> implCCThread(NULL);
        if (m_implThread)
            implCCThread = cc::ThreadImpl::createForDifferentThread(
                m_implThread->message_loop_proxy());
        ASSERT_TRUE(m_view->initialize(implCCThread.Pass()));
        m_view->setRootLayer(*m_rootLayer);
        m_view->setSurfaceReady();
    }

    virtual void TearDown()
    {
        Mock::VerifyAndClearExpectations(client());

        m_rootLayer.reset();
        m_view.reset();
    }

protected:
    scoped_ptr<WebLayer> m_rootLayer;
    scoped_ptr<WebLayerTreeViewImplForTesting> m_view;
    scoped_ptr<base::Thread> m_implThread;
};

class WebLayerTreeViewSingleThreadTest : public WebLayerTreeViewTestBase {
protected:
    void composite()
    {
        m_view->composite();
    }

    virtual void initializeCompositor() OVERRIDE
    {
    }

    virtual WebLayerTreeViewClient* client() OVERRIDE
    {
        return &m_client;
    }

    MockWebLayerTreeViewClient m_client;
};

class WebLayerTreeViewThreadedTest : public WebLayerTreeViewTestBase {
protected:
    virtual ~WebLayerTreeViewThreadedTest()
    {
    }

    void composite()
    {
        m_view->setNeedsRedraw();
        base::CancelableClosure timeout(base::Bind(&MessageLoop::Quit, base::Unretained(MessageLoop::current())));
        MessageLoop::current()->PostDelayedTask(FROM_HERE,
                                                timeout.callback(),
                                                base::TimeDelta::FromSeconds(5));
        MessageLoop::current()->Run();
        m_view->finishAllRendering();
    }

    virtual void initializeCompositor() OVERRIDE
    {
        m_implThread.reset(new base::Thread("ThreadedTest"));
        ASSERT_TRUE(m_implThread->Start());
    }

    virtual WebLayerTreeViewClient* client() OVERRIDE
    {
        return &m_client;
    }

    MockWebLayerTreeViewClientForThreadedTests m_client;
    base::CancelableClosure m_timeout;
};

} // namespace
