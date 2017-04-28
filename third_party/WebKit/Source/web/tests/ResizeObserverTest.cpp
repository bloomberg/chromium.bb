// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/ResizeObserver.h"

#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/ScriptSourceCode.h"
#include "bindings/core/v8/V8GCController.h"
#include "core/dom/ResizeObservation.h"
#include "core/dom/ResizeObserverCallback.h"
#include "core/dom/ResizeObserverController.h"
#include "core/exported/WebViewBase.h"
#include "platform/testing/UnitTestHelpers.h"
#include "platform/wtf/CurrentTime.h"
#include "public/web/WebHeap.h"
#include "web/tests/sim/SimCompositor.h"
#include "web/tests/sim/SimDisplayItemList.h"
#include "web/tests/sim/SimRequest.h"
#include "web/tests/sim/SimTest.h"

namespace blink {

namespace {

class TestResizeObserverCallback : public ResizeObserverCallback {
 public:
  TestResizeObserverCallback(Document& document)
      : document_(document), call_count_(0) {}
  void handleEvent(const HeapVector<Member<ResizeObserverEntry>>& entries,
                   ResizeObserver*) override {
    call_count_++;
  }
  ExecutionContext* GetExecutionContext() const { return document_; }
  int CallCount() const { return call_count_; }

  DEFINE_INLINE_TRACE() {
    ResizeObserverCallback::Trace(visitor);
    visitor->Trace(document_);
  }

 private:
  Member<Document> document_;
  int call_count_;
};

}  // namespace

/* Testing:
 * getTargetSize
 * setTargetSize
 * oubservationSizeOutOfSync == false
 * modify target size
 * oubservationSizeOutOfSync == true
 */
class ResizeObserverUnitTest : public SimTest {};

TEST_F(ResizeObserverUnitTest, ResizeObservationSize) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");

  main_resource.Start();
  main_resource.Write(
      "<div id='domTarget' style='width:100px;height:100px'>yo</div>"
      "<svg height='200' width='200'>"
      "<circle id='svgTarget' cx='100' cy='100' r='100'/>"
      "</svg>");
  main_resource.Finish();

  ResizeObserverCallback* callback =
      new TestResizeObserverCallback(GetDocument());
  ResizeObserver* observer = ResizeObserver::Create(GetDocument(), callback);
  Element* dom_target = GetDocument().getElementById("domTarget");
  Element* svg_target = GetDocument().getElementById("svgTarget");
  ResizeObservation* dom_observation =
      new ResizeObservation(dom_target, observer);
  ResizeObservation* svg_observation =
      new ResizeObservation(svg_target, observer);

  // Initial observation is out of sync
  ASSERT_TRUE(dom_observation->ObservationSizeOutOfSync());
  ASSERT_TRUE(svg_observation->ObservationSizeOutOfSync());

  // Target size is correct
  LayoutSize size = dom_observation->ComputeTargetSize();
  ASSERT_EQ(size.Width(), 100);
  ASSERT_EQ(size.Height(), 100);
  dom_observation->SetObservationSize(size);

  size = svg_observation->ComputeTargetSize();
  ASSERT_EQ(size.Width(), 200);
  ASSERT_EQ(size.Height(), 200);
  svg_observation->SetObservationSize(size);

  // Target size is in sync
  ASSERT_FALSE(dom_observation->ObservationSizeOutOfSync());

  // Target depths
  ASSERT_EQ(svg_observation->TargetDepth() - dom_observation->TargetDepth(),
            (size_t)1);
}

TEST_F(ResizeObserverUnitTest, TestMemoryLeaks) {
  ResizeObserverController& controller =
      GetDocument().EnsureResizeObserverController();
  const HeapHashSet<WeakMember<ResizeObserver>>& observers =
      controller.Observers();
  ASSERT_EQ(observers.size(), 0U);
  v8::HandleScope scope(v8::Isolate::GetCurrent());

  ScriptController& script_controller =
      GetDocument().ExecutingFrame()->GetScriptController();

  //
  // Test whether ResizeObserver is kept alive by direct JS reference
  //
  script_controller.ExecuteScriptInMainWorldAndReturnValue(
      ScriptSourceCode("var ro = new ResizeObserver( entries => {});"),
      ScriptController::kExecuteScriptWhenScriptsDisabled);
  ASSERT_EQ(observers.size(), 1U);
  script_controller.ExecuteScriptInMainWorldAndReturnValue(
      ScriptSourceCode("ro = undefined;"),
      ScriptController::kExecuteScriptWhenScriptsDisabled);
  V8GCController::CollectAllGarbageForTesting(v8::Isolate::GetCurrent());
  WebHeap::CollectAllGarbageForTesting();
  ASSERT_EQ(observers.IsEmpty(), true);

  //
  // Test whether ResizeObserver is kept alive by an Element
  //
  script_controller.ExecuteScriptInMainWorldAndReturnValue(
      ScriptSourceCode("var ro = new ResizeObserver( () => {});"
                       "var el = document.createElement('div');"
                       "ro.observe(el);"
                       "ro = undefined;"),
      ScriptController::kExecuteScriptWhenScriptsDisabled);
  ASSERT_EQ(observers.size(), 1U);
  V8GCController::CollectAllGarbageForTesting(v8::Isolate::GetCurrent());
  WebHeap::CollectAllGarbageForTesting();
  ASSERT_EQ(observers.size(), 1U);
  script_controller.ExecuteScriptInMainWorldAndReturnValue(
      ScriptSourceCode("el = undefined;"),
      ScriptController::kExecuteScriptWhenScriptsDisabled);
  V8GCController::CollectAllGarbageForTesting(v8::Isolate::GetCurrent());
  WebHeap::CollectAllGarbageForTesting();
  ASSERT_EQ(observers.IsEmpty(), true);
}

}  // namespace blink
