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
#include "third_party/blink/renderer/modules/csspaint/paint_worklet.h"
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

  void RunAddGlobalScopesTest(WorkerThread* thread,
                              PaintWorkletProxyClient* proxy_client,
                              base::WaitableEvent* waitable_event) {
    // For this test, we cheat and reuse the same global scope object from a
    // single WorkerThread. In real code these would be different global scopes.

    // First, add all but one of the global scopes. The proxy client should not
    // yet register itself.
    for (size_t i = 0; i < PaintWorklet::kNumGlobalScopes - 1; i++) {
      proxy_client->AddGlobalScope(
          To<WorkletGlobalScope>(thread->GlobalScope()));
    }

    EXPECT_EQ(proxy_client->global_scopes_.size(),
              PaintWorklet::kNumGlobalScopes - 1);
    EXPECT_EQ(dispatcher_->painter_map_.size(), 0u);

    // Now add the final global scope. This should trigger the registration.
    proxy_client->AddGlobalScope(To<WorkletGlobalScope>(thread->GlobalScope()));
    EXPECT_EQ(proxy_client->global_scopes_.size(),
              PaintWorklet::kNumGlobalScopes);
    EXPECT_EQ(dispatcher_->painter_map_.size(), 1u);

    waitable_event->Signal();
  }

  using TestCallback =
      void (PaintWorkletProxyClientTest::*)(WorkerThread*,
                                            PaintWorkletProxyClient*,
                                            base::WaitableEvent*);

  void RunMultipleGlobalScopeTestsOnWorklet(TestCallback callback) {
    // PaintWorklet is stateless, and this is enforced via having multiple
    // global scopes (which are switched between). To mimic the real world,
    // create multiple WorkerThread for this. Note that the underlying thread
    // may be shared even though they are unique WorkerThread instances!
    Vector<std::unique_ptr<WorkerThread>> worklet_threads;
    for (size_t i = 0; i < PaintWorklet::kNumGlobalScopes; i++) {
      worklet_threads.push_back(CreateThreadAndProvidePaintWorkletProxyClient(
          &GetDocument(), reporting_proxy_.get(), proxy_client_));
    }

    // Now let the test actually run. We only run the test on the first worklet
    // thread currently; this suffices since they share the proxy.
    base::WaitableEvent waitable_event;
    PostCrossThreadTask(
        *worklet_threads[0]->GetTaskRunner(TaskType::kInternalTest), FROM_HERE,
        CrossThreadBind(
            callback, CrossThreadUnretained(this),
            CrossThreadUnretained(worklet_threads[0].get()),
            CrossThreadPersistent<PaintWorkletProxyClient>(proxy_client_),
            CrossThreadUnretained(&waitable_event)));
    waitable_event.Wait();
    waitable_event.Reset();

    // And finally clean up.
    for (size_t i = 0; i < PaintWorklet::kNumGlobalScopes; i++) {
      worklet_threads[i]->Terminate();
      worklet_threads[i]->WaitForShutdownForTesting();
    }
  }

  void RunPaintOnWorklet(WorkerThread* thread,
                         PaintWorkletProxyClient* proxy_client,
                         base::WaitableEvent* waitable_event) {
    // Make sure to register the painter on all global scopes.
    for (const auto& global_scope : proxy_client->GetGlobalScopesForTesting()) {
      global_scope->ScriptController()->Evaluate(
          ScriptSourceCode("registerPaint('foo', class { paint() { } });"),
          SanitizeScriptErrors::kDoNotSanitize);
    }

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

TEST_F(PaintWorkletProxyClientTest, AddGlobalScope) {
  ScopedOffMainThreadCSSPaintForTest off_main_thread_css_paint(true);
  // Global scopes must be created on worker threads.
  std::unique_ptr<WorkerThread> worklet_thread =
      CreateThreadAndProvidePaintWorkletProxyClient(
          &GetDocument(), reporting_proxy_.get(), proxy_client_);

  EXPECT_TRUE(proxy_client_->GetGlobalScopesForTesting().IsEmpty());

  base::WaitableEvent waitable_event;
  PostCrossThreadTask(
      *worklet_thread->GetTaskRunner(TaskType::kInternalTest), FROM_HERE,
      CrossThreadBind(
          &PaintWorkletProxyClientTest::RunAddGlobalScopesTest,
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
  RunMultipleGlobalScopeTestsOnWorklet(
      &PaintWorkletProxyClientTest::RunPaintOnWorklet);
}

}  // namespace blink
