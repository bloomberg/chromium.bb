// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/display_lock/display_lock_context.h"

#include "base/run_loop.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_display_lock_callback.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_gc_controller.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

class DisplayLockContextTest : public RenderingTest {
 public:
  void SetUp() override {
    RenderingTest::SetUp();
    features_backup_.emplace();
    RuntimeEnabledFeatures::SetDisplayLockingEnabled(true);
  }

  void TearDown() override {
    if (features_backup_) {
      features_backup_->Restore();
      features_backup_.reset();
    }
  }

  static void RunPendingTasks() {
    base::RunLoop run_loop;
    Thread::Current()->GetTaskRunner()->PostTask(
        FROM_HERE, run_loop.QuitWhenIdleClosure());
    run_loop.Run();
  }

  DisplayLockContext::State ContextState(DisplayLockContext* context) const {
    return context->state_;
  }

 private:
  base::Optional<RuntimeEnabledFeatures::Backup> features_backup_;
};

class ContextTestEmptyFunction : public ScriptFunction {
 public:
  static v8::Local<v8::Function> Create(ScriptState* script_state) {
    auto* callback =
        MakeGarbageCollected<ContextTestEmptyFunction>(script_state);
    return callback->BindToV8Function();
  }

  ContextTestEmptyFunction(ScriptState* script_state)
      : ScriptFunction(script_state) {}

  ScriptValue Call(ScriptValue value) final { return value; }
};

TEST_F(DisplayLockContextTest, ContextCleanedUpWhenElementDestroyed) {
  GetDocument().GetFrame()->GetSettings()->SetScriptEnabled(true);

  WeakPersistent<Element> element =
      GetDocument().CreateRawElement(html_names::kDivTag);

  ScriptPromise script_promise;
  {
    auto* script_state = ToScriptStateForMainWorld(GetDocument().GetFrame());
    ScriptState::Scope scope(script_state);
    script_promise = element->acquireDisplayLock(
        script_state, V8DisplayLockCallback::Create(
                          ContextTestEmptyFunction::Create(script_state)));
  }
  WeakPersistent<DisplayLockContext> context = element->GetDisplayLockContext();
  ASSERT_TRUE(context);

  // Ensure we get to the disconnected state.
  RunPendingTasks();
  EXPECT_EQ(ContextState(context), DisplayLockContext::kDisconnected);

  // This should be able to clean up the element.
  ThreadState::Current()->CollectAllGarbage();
  EXPECT_FALSE(element);

  // Now that the element is gone, HasPendingActivity should return false for
  // the context.
  EXPECT_FALSE(context->HasPendingActivity());

  // That means the following garbage collection should be able to clean up the
  // context.
  V8GCController::CollectAllGarbageForTesting(
      v8::Isolate::GetCurrent(),
      v8::EmbedderHeapTracer::EmbedderStackState::kEmpty);
  ThreadState::Current()->CollectAllGarbage();
  EXPECT_FALSE(context);
}
}  // namespace blink
