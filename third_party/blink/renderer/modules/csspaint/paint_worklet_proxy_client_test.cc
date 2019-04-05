// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/csspaint/paint_worklet_proxy_client.h"

#include <memory>
#include <utility>

#include "base/synchronization/waitable_event.h"
#include "base/test/test_simple_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/script_source_code.h"
#include "third_party/blink/renderer/bindings/core/v8/worker_or_worklet_script_controller.h"
#include "third_party/blink/renderer/core/css/cssom/paint_worklet_input.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/core/workers/worker_reporting_proxy.h"
#include "third_party/blink/renderer/modules/csspaint/paint_worklet_global_scope.h"
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

  void SetGlobalScopeForTesting(WorkerThread* thread,
                                PaintWorkletProxyClient* proxy_client,
                                base::WaitableEvent* waitable_event) {
    proxy_client->SetGlobalScopeForTesting(
        static_cast<PaintWorkletGlobalScope*>(
            To<WorkletGlobalScope>(thread->GlobalScope())));
    waitable_event->Signal();
  }

  using TestCallback =
      void (PaintWorkletProxyClientTest::*)(WorkerThread*,
                                            PaintWorkletProxyClient*,
                                            base::WaitableEvent*);
  void RunTestOnWorkletThread(TestCallback callback) {
    std::unique_ptr<WorkerThread> worklet =
        CreateThreadAndProvidePaintWorkletProxyClient(
            &GetDocument(), reporting_proxy_.get(), proxy_client_);

    base::WaitableEvent waitable_event;
    PostCrossThreadTask(
        *worklet->GetTaskRunner(TaskType::kInternalTest), FROM_HERE,
        CrossThreadBind(
            callback, CrossThreadUnretained(this),
            CrossThreadUnretained(worklet.get()),
            CrossThreadPersistent<PaintWorkletProxyClient>(proxy_client_),
            CrossThreadUnretained(&waitable_event)));
    waitable_event.Wait();
    waitable_event.Reset();

    worklet->Terminate();
    worklet->WaitForShutdownForTesting();
  }

  void RunPaintOnWorklet(WorkerThread* thread,
                         PaintWorkletProxyClient* proxy_client,
                         base::WaitableEvent* waitable_event) {
    // The "registerPaint" script calls the real SetGlobalScope, so at this
    // moment all we need is setting the |global_scope_| without any other
    // things.
    proxy_client->SetGlobalScopeForTesting(
        static_cast<PaintWorkletGlobalScope*>(
            To<WorkletGlobalScope>(thread->GlobalScope())));
    CrossThreadPersistent<PaintWorkletGlobalScope> global_scope =
        proxy_client->global_scope_;
    global_scope->ScriptController()->Evaluate(
        ScriptSourceCode("registerPaint('foo', class { paint() { } });"),
        SanitizeScriptErrors::kDoNotSanitize);

    PaintWorkletStylePropertyMap::CrossThreadData data;
    scoped_refptr<PaintWorkletInput> input =
        base::MakeRefCounted<PaintWorkletInput>("foo", FloatSize(100, 100),
                                                1.0f, 1, std::move(data));
    sk_sp<PaintRecord> record = proxy_client->Paint(input.get());
    EXPECT_NE(record, nullptr);

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

TEST_F(PaintWorkletProxyClientTest, Paint) {
  ScopedOffMainThreadCSSPaintForTest off_main_thread_css_paint(true);
  RunTestOnWorkletThread(&PaintWorkletProxyClientTest::RunPaintOnWorklet);
}

}  // namespace blink
