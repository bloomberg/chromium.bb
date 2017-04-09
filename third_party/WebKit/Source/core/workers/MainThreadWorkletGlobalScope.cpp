// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/MainThreadWorkletGlobalScope.h"

#include "bindings/core/v8/ScriptSourceCode.h"
#include "bindings/core/v8/WorkerOrWorkletScriptController.h"
#include "core/frame/Deprecation.h"
#include "core/frame/FrameConsole.h"
#include "core/frame/LocalFrame.h"
#include "core/inspector/MainThreadDebugger.h"

namespace blink {

MainThreadWorkletGlobalScope::MainThreadWorkletGlobalScope(
    LocalFrame* frame,
    const KURL& url,
    const String& user_agent,
    PassRefPtr<SecurityOrigin> security_origin,
    v8::Isolate* isolate)
    : WorkletGlobalScope(url, user_agent, std::move(security_origin), isolate),
      ContextClient(frame) {}

MainThreadWorkletGlobalScope::~MainThreadWorkletGlobalScope() {}

void MainThreadWorkletGlobalScope::CountFeature(UseCounter::Feature feature) {
  DCHECK(IsMainThread());
  // A parent document is on the same thread, so just record API use in the
  // document's UseCounter.
  UseCounter::Count(GetFrame(), feature);
}

void MainThreadWorkletGlobalScope::CountDeprecation(
    UseCounter::Feature feature) {
  DCHECK(IsMainThread());
  // A parent document is on the same thread, so just record API use in the
  // document's UseCounter.
  AddDeprecationMessage(feature);
  Deprecation::CountDeprecation(GetFrame(), feature);
}

WorkerThread* MainThreadWorkletGlobalScope::GetThread() const {
  NOTREACHED();
  return nullptr;
}

void MainThreadWorkletGlobalScope::EvaluateScript(
    const ScriptSourceCode& script_source_code) {
  ScriptController()->Evaluate(script_source_code);
}

void MainThreadWorkletGlobalScope::TerminateWorkletGlobalScope() {
  Dispose();
}

void MainThreadWorkletGlobalScope::AddConsoleMessage(
    ConsoleMessage* console_message) {
  GetFrame()->Console().AddMessage(console_message);
}

void MainThreadWorkletGlobalScope::ExceptionThrown(ErrorEvent* event) {
  MainThreadDebugger::Instance()->ExceptionThrown(this, event);
}

}  // namespace blink
