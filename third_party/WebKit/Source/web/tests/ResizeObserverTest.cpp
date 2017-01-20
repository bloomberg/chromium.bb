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
#include "platform/testing/UnitTestHelpers.h"
#include "public/web/WebHeap.h"
#include "web/WebViewImpl.h"
#include "web/tests/sim/SimCompositor.h"
#include "web/tests/sim/SimDisplayItemList.h"
#include "web/tests/sim/SimRequest.h"
#include "web/tests/sim/SimTest.h"
#include "wtf/CurrentTime.h"

namespace blink {

namespace {

class TestResizeObserverCallback : public ResizeObserverCallback {
 public:
  TestResizeObserverCallback(Document& document)
      : m_document(document), m_callCount(0) {}
  void handleEvent(const HeapVector<Member<ResizeObserverEntry>>& entries,
                   ResizeObserver*) override {
    m_callCount++;
  }
  ExecutionContext* getExecutionContext() const { return m_document; }
  int callCount() const { return m_callCount; }

  DEFINE_INLINE_TRACE() {
    ResizeObserverCallback::trace(visitor);
    visitor->trace(m_document);
  }

 private:
  Member<Document> m_document;
  int m_callCount;
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
  SimRequest mainResource("https://example.com/", "text/html");
  loadURL("https://example.com/");

  mainResource.start();
  mainResource.write(
      "<div id='domTarget' style='width:100px;height:100px'>yo</div>"
      "<svg height='200' width='200'>"
      "<circle id='svgTarget' cx='100' cy='100' r='100'/>"
      "</svg>");
  mainResource.finish();

  ResizeObserverCallback* callback = new TestResizeObserverCallback(document());
  ResizeObserver* observer = ResizeObserver::create(document(), callback);
  Element* domTarget = document().getElementById("domTarget");
  Element* svgTarget = document().getElementById("svgTarget");
  ResizeObservation* domObservation =
      new ResizeObservation(domTarget, observer);
  ResizeObservation* svgObservation =
      new ResizeObservation(svgTarget, observer);

  // Initial observation is out of sync
  ASSERT_TRUE(domObservation->observationSizeOutOfSync());
  ASSERT_TRUE(svgObservation->observationSizeOutOfSync());

  // Target size is correct
  LayoutSize size = domObservation->computeTargetSize();
  ASSERT_EQ(size.width(), 100);
  ASSERT_EQ(size.height(), 100);
  domObservation->setObservationSize(size);

  size = svgObservation->computeTargetSize();
  ASSERT_EQ(size.width(), 200);
  ASSERT_EQ(size.height(), 200);
  svgObservation->setObservationSize(size);

  // Target size is in sync
  ASSERT_FALSE(domObservation->observationSizeOutOfSync());

  // Target depths
  ASSERT_EQ(svgObservation->targetDepth() - domObservation->targetDepth(),
            (size_t)1);
}

TEST_F(ResizeObserverUnitTest, TestMemoryLeaks) {
  ResizeObserverController& controller =
      document().ensureResizeObserverController();
  const HeapHashSet<WeakMember<ResizeObserver>>& observers =
      controller.observers();
  ASSERT_EQ(observers.size(), 0U);
  v8::HandleScope scope(v8::Isolate::GetCurrent());

  ScriptController& script = document().executingFrame()->script();

  //
  // Test whether ResizeObserver is kept alive by direct JS reference
  //
  script.executeScriptInMainWorldAndReturnValue(
      ScriptSourceCode("var ro = new ResizeObserver( entries => {});"),
      ScriptController::ExecuteScriptWhenScriptsDisabled);
  ASSERT_EQ(observers.size(), 1U);
  script.executeScriptInMainWorldAndReturnValue(
      ScriptSourceCode("ro = undefined;"),
      ScriptController::ExecuteScriptWhenScriptsDisabled);
  V8GCController::collectAllGarbageForTesting(v8::Isolate::GetCurrent());
  WebHeap::collectAllGarbageForTesting();
  ASSERT_EQ(observers.isEmpty(), true);

  //
  // Test whether ResizeObserver is kept alive by an Element
  //
  script.executeScriptInMainWorldAndReturnValue(
      ScriptSourceCode("var ro = new ResizeObserver( () => {});"
                       "var el = document.createElement('div');"
                       "ro.observe(el);"
                       "ro = undefined;"),
      ScriptController::ExecuteScriptWhenScriptsDisabled);
  ASSERT_EQ(observers.size(), 1U);
  V8GCController::collectAllGarbageForTesting(v8::Isolate::GetCurrent());
  WebHeap::collectAllGarbageForTesting();
  ASSERT_EQ(observers.size(), 1U);
  script.executeScriptInMainWorldAndReturnValue(
      ScriptSourceCode("el = undefined;"),
      ScriptController::ExecuteScriptWhenScriptsDisabled);
  V8GCController::collectAllGarbageForTesting(v8::Isolate::GetCurrent());
  WebHeap::collectAllGarbageForTesting();
  ASSERT_EQ(observers.isEmpty(), true);
}

}  // namespace blink
