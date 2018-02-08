// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/csspaint/PaintWorklet.h"

#include <memory>
#include "bindings/core/v8/ScriptSourceCode.h"
#include "bindings/core/v8/V8GCController.h"
#include "bindings/core/v8/WorkerOrWorkletScriptController.h"
#include "core/frame/LocalFrame.h"
#include "core/layout/LayoutView.h"
#include "core/testing/PageTestBase.h"
#include "modules/csspaint/CSSPaintDefinition.h"
#include "modules/csspaint/PaintWorkletGlobalScope.h"
#include "modules/csspaint/PaintWorkletGlobalScopeProxy.h"
#include "platform/wtf/CryptographicallyRandomNumber.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
class TestPaintWorklet : public PaintWorklet {
 public:
  explicit TestPaintWorklet(LocalFrame* frame) : PaintWorklet(frame) {}

  void SetPaintsToSwitch(size_t num) { paints_to_switch_ = num; }

  int GetPaintsBeforeSwitching() override { return paints_to_switch_; }

  // We always switch to another global scope so that we can tell how often it
  // was switched in the test.
  size_t SelectNewGlobalScope() override {
    return (GetActiveGlobalScopeForTesting() + 1) %
           PaintWorklet::kNumGlobalScopes;
  }

  size_t GetActiveGlobalScope() { return GetActiveGlobalScopeForTesting(); }

 private:
  size_t paints_to_switch_;
};

class PaintWorkletTest : public PageTestBase {
 public:
  void SetUp() override {
    PageTestBase::SetUp(IntSize());
    test_paint_worklet_ =
        new TestPaintWorklet(GetDocument().domWindow()->GetFrame());
    proxy_ = test_paint_worklet_->CreateGlobalScope();
  }

  TestPaintWorklet* GetTestPaintWorklet() { return test_paint_worklet_.Get(); }

  size_t SelectGlobalScope(TestPaintWorklet* paint_worklet) {
    return paint_worklet->SelectGlobalScope();
  }

  PaintWorkletGlobalScopeProxy* GetProxy() {
    return PaintWorkletGlobalScopeProxy::From(proxy_.Get());
  }

  ImageResourceObserver* GetImageResourceObserver() {
    return GetDocument().domWindow()->GetFrame()->ContentLayoutObject();
  }

  // Helper function used in GlobalScopeSelection test.
  void ExpectSwitchGlobalScope(bool expect_switch_within_frame,
                               size_t num_paint_calls,
                               size_t paint_cnt_to_switch,
                               size_t expected_num_paints_before_switch,
                               TestPaintWorklet* paint_worklet_to_test) {
    paint_worklet_to_test->GetFrame()->View()->UpdateAllLifecyclePhases();
    paint_worklet_to_test->SetPaintsToSwitch(paint_cnt_to_switch);
    size_t previously_selected_global_scope =
        paint_worklet_to_test->GetActiveGlobalScope();
    size_t global_scope_switch_count = 0u;

    // How many paint calls are there before we switch to another global scope.
    // Because the first paint call in each frame doesn't count as switching,
    // a result of 0 means there is not switching in that frame.
    size_t num_paints_before_switch = 0u;
    for (size_t j = 0; j < num_paint_calls; j++) {
      size_t selected_global_scope = SelectGlobalScope(paint_worklet_to_test);
      if (j == 0) {
        EXPECT_NE(selected_global_scope, previously_selected_global_scope);
      } else if (selected_global_scope != previously_selected_global_scope) {
        num_paints_before_switch = j + 1;
        global_scope_switch_count++;
      }
      previously_selected_global_scope = selected_global_scope;
    }
    EXPECT_LT(global_scope_switch_count, 2u);
    EXPECT_EQ(num_paints_before_switch, expected_num_paints_before_switch);
  }

  void Terminate() {
    proxy_->TerminateWorkletGlobalScope();
    proxy_ = nullptr;
  }

 private:
  Persistent<WorkletGlobalScopeProxy> proxy_;
  Persistent<TestPaintWorklet> test_paint_worklet_;
};

TEST_F(PaintWorkletTest, GarbageCollectionOfCSSPaintDefinition) {
  PaintWorkletGlobalScope* global_scope = GetProxy()->global_scope();
  global_scope->ScriptController()->Evaluate(
      ScriptSourceCode("registerPaint('foo', class { paint() { } });"));

  CSSPaintDefinition* definition = global_scope->FindDefinition("foo");
  DCHECK(definition);

  v8::Isolate* isolate =
      global_scope->ScriptController()->GetScriptState()->GetIsolate();
  DCHECK(isolate);

  // Set our ScopedPersistent to the paint function, and make weak.
  ScopedPersistent<v8::Function> handle;
  {
    v8::HandleScope handle_scope(isolate);
    handle.Set(isolate, definition->PaintFunctionForTesting(isolate));
    handle.SetPhantom();
  }
  DCHECK(!handle.IsEmpty());
  DCHECK(handle.IsWeak());

  // Run a GC, persistent shouldn't have been collected yet.
  ThreadState::Current()->CollectAllGarbage();
  V8GCController::CollectAllGarbageForTesting(isolate);
  DCHECK(!handle.IsEmpty());

  // Delete the page & associated objects.
  Terminate();

  // Run a GC, the persistent should have been collected.
  ThreadState::Current()->CollectAllGarbage();
  V8GCController::CollectAllGarbageForTesting(isolate);
  DCHECK(handle.IsEmpty());
}

// This is a crash test for crbug.com/803026. At some point, we shipped the
// CSSPaintAPI without shipping the CSSPaintAPIArguments, the result of it is
// that the |paint_arguments| in the CSSPaintDefinition::Paint() becomes
// nullptr and the renderer crashes. This is a regression test to ensure that
// we will never crash.
TEST_F(PaintWorkletTest, PaintWithNullPaintArguments) {
  PaintWorkletGlobalScope* global_scope = GetProxy()->global_scope();
  global_scope->ScriptController()->Evaluate(
      ScriptSourceCode("registerPaint('foo', class { paint() { } });"));

  CSSPaintDefinition* definition = global_scope->FindDefinition("foo");
  ASSERT_TRUE(definition);

  ImageResourceObserver* observer = GetImageResourceObserver();
  ASSERT_TRUE(observer);

  const IntSize container_size(100, 100);
  scoped_refptr<Image> image =
      definition->Paint(*observer, container_size, nullptr);
  EXPECT_NE(image, nullptr);
}

// In this test, we have only one global scope, which means registerPaint is
// called only once, and hence we have only one document paint definition
// registered. In the real world, this document paint definition should not be
// used to paint until we see a second one being registed with the same name.
TEST_F(PaintWorkletTest, SinglyRegisteredDocumentDefinitionNotUsed) {
  PaintWorkletGlobalScope* global_scope = GetProxy()->global_scope();
  global_scope->ScriptController()->Evaluate(
      ScriptSourceCode("registerPaint('foo', class { paint() { } });"));

  CSSPaintImageGeneratorImpl* generator =
      static_cast<CSSPaintImageGeneratorImpl*>(
          CSSPaintImageGeneratorImpl::Create("foo", GetDocument(), nullptr));
  EXPECT_TRUE(generator);
  EXPECT_EQ(generator->GetRegisteredDefinitionCountForTesting(), 1u);
  DocumentPaintDefinition* definition;
  EXPECT_FALSE(generator->GetValidDocumentDefinitionForTesting(definition));
  EXPECT_FALSE(definition);
}

// In this test, we set a list of "paints_to_switch" numbers, and in each frame,
// we switch to a new global scope when the number of paint calls is >= the
// corresponding number.
TEST_F(PaintWorkletTest, GlobalScopeSelection) {
  TestPaintWorklet* paint_worklet_to_test = GetTestPaintWorklet();

  ExpectSwitchGlobalScope(false, 5, 1, 0, paint_worklet_to_test);
  ExpectSwitchGlobalScope(true, 15, 10, 10, paint_worklet_to_test);
  // In the last one where |paints_to_switch| is 20, there is no switching after
  // the first paint call.
  ExpectSwitchGlobalScope(false, 10, 20, 0, paint_worklet_to_test);

  // Delete the page & associated objects.
  Terminate();
}

}  // namespace blink
