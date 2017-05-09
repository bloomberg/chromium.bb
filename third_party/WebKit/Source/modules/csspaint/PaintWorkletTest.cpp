// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/csspaint/PaintWorklet.h"

#include "bindings/core/v8/ScriptSourceCode.h"
#include "bindings/core/v8/V8GCController.h"
#include "bindings/core/v8/WorkerOrWorkletScriptController.h"
#include "core/frame/LocalFrame.h"
#include "core/testing/DummyPageHolder.h"
#include "modules/csspaint/CSSPaintDefinition.h"
#include "modules/csspaint/PaintWorkletGlobalScope.h"
#include "modules/csspaint/WindowPaintWorklet.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <memory>

namespace blink {

class PaintWorkletTest : public testing::Test {
 public:
  PaintWorkletTest() : page_(DummyPageHolder::Create()) {}

  PaintWorklet* GetPaintWorklet() {
    return WindowPaintWorklet::From(*page_->GetFrame().DomWindow())
        .paintWorklet();
  }

 protected:
  std::unique_ptr<DummyPageHolder> page_;
};

TEST_F(PaintWorkletTest, GarbageCollectionOfCSSPaintDefinition) {
  PaintWorkletGlobalScopeProxy* proxy = PaintWorkletGlobalScopeProxy::From(
      GetPaintWorklet()->GetWorkletGlobalScopeProxy());
  PaintWorkletGlobalScope* global_scope = proxy->global_scope();
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
  page_.reset();

  // Run a GC, the persistent should have been collected.
  ThreadState::Current()->CollectAllGarbage();
  V8GCController::CollectAllGarbageForTesting(isolate);
  DCHECK(handle.IsEmpty());
}

}  // namespace blink
