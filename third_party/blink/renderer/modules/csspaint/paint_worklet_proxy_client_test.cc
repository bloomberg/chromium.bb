// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/csspaint/paint_worklet_proxy_client.h"

#include <memory>
#include <utility>

#include "base/synchronization/waitable_event.h"
#include "base/test/test_simple_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/core/workers/worker_reporting_proxy.h"
#include "third_party/blink/renderer/modules/worklet/worklet_thread_test_common.h"
#include "third_party/blink/renderer/platform/graphics/paint_worklet_paint_dispatcher.h"

namespace blink {

class PaintWorkletProxyClientTest : public RenderingTest {
 public:
  PaintWorkletProxyClientTest() = default;

  void SetUp() override {
    RenderingTest::SetUp();
    dispatcher_ = base::MakeRefCounted<PaintWorkletPaintDispatcher>();

    proxy_client_ =
        MakeGarbageCollected<PaintWorkletProxyClient>(1, dispatcher_);
    reporting_proxy_ = std::make_unique<WorkerReportingProxy>();
  }

  void SetAndCheckGlobalScope(WorkerThread* thread,
                              PaintWorkletProxyClient* proxy_client,
                              base::WaitableEvent* waitable_event) {
    proxy_client->SetGlobalScope(To<WorkletGlobalScope>(thread->GlobalScope()));
    EXPECT_EQ(proxy_client->global_scope_,
              To<WorkletGlobalScope>(thread->GlobalScope()));
    EXPECT_EQ(dispatcher_->painter_map_.size(), 1u);
    waitable_event->Signal();
  }

  scoped_refptr<PaintWorkletPaintDispatcher> dispatcher_;
  Persistent<PaintWorkletProxyClient> proxy_client_;
  std::unique_ptr<WorkerReportingProxy> reporting_proxy_;
};

TEST_F(PaintWorkletProxyClientTest, PaintWorkletProxyClientConstruction) {
  PaintWorkletProxyClient* proxy_client =
      MakeGarbageCollected<PaintWorkletProxyClient>(1, nullptr);
  EXPECT_EQ(proxy_client->worklet_id_, 1);
  EXPECT_EQ(proxy_client->compositor_paintee_, nullptr);

  scoped_refptr<PaintWorkletPaintDispatcher> dispatcher =
      base::MakeRefCounted<PaintWorkletPaintDispatcher>();

  proxy_client =
      MakeGarbageCollected<PaintWorkletProxyClient>(1, std::move(dispatcher));
  EXPECT_EQ(proxy_client->worklet_id_, 1);
  EXPECT_NE(proxy_client->compositor_paintee_, nullptr);
}

TEST_F(PaintWorkletProxyClientTest, SetGlobalScope) {
  ScopedOffMainThreadCSSPaintForTest off_main_thread_css_paint(true);
  // Global scopes must be created on worker threads.
  std::unique_ptr<WorkerThread> worklet_thread =
      CreateThreadAndProvidePaintWorkletProxyClient(
          &GetDocument(), reporting_proxy_.get(), proxy_client_);

  EXPECT_EQ(proxy_client_->global_scope_, nullptr);

  // Register global scopes with proxy client. This step must be performed on
  // the worker threads.
  base::WaitableEvent waitable_event;
  PostCrossThreadTask(
      *worklet_thread->GetTaskRunner(TaskType::kInternalTest), FROM_HERE,
      CrossThreadBind(
          &PaintWorkletProxyClientTest::SetAndCheckGlobalScope,
          CrossThreadUnretained(this),
          CrossThreadUnretained(worklet_thread.get()),
          CrossThreadPersistent<PaintWorkletProxyClient>(proxy_client_),
          CrossThreadUnretained(&waitable_event)));
  waitable_event.Wait();

  worklet_thread->Terminate();
  worklet_thread->WaitForShutdownForTesting();
}

}  // namespace blink
