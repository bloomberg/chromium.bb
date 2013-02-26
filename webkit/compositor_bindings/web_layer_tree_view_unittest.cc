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

class MockWebLayerTreeViewClientForThreadedTests :
    public MockWebLayerTreeViewClient {
 public:
  virtual void didBeginFrame() OVERRIDE {
    MessageLoop::current()->Quit();
    MockWebLayerTreeViewClient::didBeginFrame();
  }
};

class WebLayerTreeViewTestBase : public Test {
 protected:
  virtual void initializeCompositor() = 0;
  virtual WebLayerTreeViewClient* client() = 0;

 public:
  virtual void SetUp() {
    initializeCompositor();
    root_layer_.reset(new WebLayerImpl);
    view_.reset(new WebLayerTreeViewImplForTesting(
        WebLayerTreeViewImplForTesting::FAKE_CONTEXT, client()));
    scoped_ptr<cc::Thread> impl_cc_thread(NULL);
    if (impl_thread_)
      impl_cc_thread = cc::ThreadImpl::createForDifferentThread(
          impl_thread_->message_loop_proxy());
    ASSERT_TRUE(view_->initialize(impl_cc_thread.Pass()));
    view_->setRootLayer(*root_layer_);
    view_->setSurfaceReady();
  }

  virtual void TearDown() {
    Mock::VerifyAndClearExpectations(client());

    root_layer_.reset();
    view_.reset();
  }

 protected:
  scoped_ptr<WebLayer> root_layer_;
  scoped_ptr<WebLayerTreeViewImplForTesting> view_;
  scoped_ptr<base::Thread> impl_thread_;
};

class WebLayerTreeViewSingleThreadTest : public WebLayerTreeViewTestBase {
 protected:
  void composite() { view_->composite(); }

  virtual void initializeCompositor() OVERRIDE {}

  virtual WebLayerTreeViewClient* client() OVERRIDE { return &client_; }

  MockWebLayerTreeViewClient client_;
};

class WebLayerTreeViewThreadedTest : public WebLayerTreeViewTestBase {
 protected:
  virtual ~WebLayerTreeViewThreadedTest() {}

  void composite() {
    view_->setNeedsRedraw();
    base::CancelableClosure timeout(base::Bind(
        &MessageLoop::Quit, base::Unretained(MessageLoop::current())));
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE, timeout.callback(), base::TimeDelta::FromSeconds(5));
    MessageLoop::current()->Run();
    view_->finishAllRendering();
  }

  virtual void initializeCompositor() OVERRIDE {
    impl_thread_.reset(new base::Thread("ThreadedTest"));
    ASSERT_TRUE(impl_thread_->Start());
  }

  virtual WebLayerTreeViewClient* client() OVERRIDE { return &client_; }

  MockWebLayerTreeViewClientForThreadedTests client_;
  base::CancelableClosure timeout_;
};

}  // namespace
