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
    const String& userAgent,
    PassRefPtr<SecurityOrigin> securityOrigin,
    v8::Isolate* isolate)
    : WorkletGlobalScope(url, userAgent, std::move(securityOrigin), isolate),
      ContextClient(frame) {}

MainThreadWorkletGlobalScope::~MainThreadWorkletGlobalScope() {}

void MainThreadWorkletGlobalScope::countFeature(UseCounter::Feature feature) {
  DCHECK(isMainThread());
  // A parent document is on the same thread, so just record API use in the
  // document's UseCounter.
  UseCounter::count(frame(), feature);
}

void MainThreadWorkletGlobalScope::countDeprecation(
    UseCounter::Feature feature) {
  DCHECK(isMainThread());
  // A parent document is on the same thread, so just record API use in the
  // document's UseCounter.
  addDeprecationMessage(feature);
  Deprecation::countDeprecation(frame(), feature);
}

WorkerThread* MainThreadWorkletGlobalScope::thread() const {
  NOTREACHED();
  return nullptr;
}

void MainThreadWorkletGlobalScope::evaluateScript(
    const ScriptSourceCode& scriptSourceCode) {
  scriptController()->evaluate(scriptSourceCode);
}

void MainThreadWorkletGlobalScope::terminateWorkletGlobalScope() {
  dispose();
}

void MainThreadWorkletGlobalScope::addConsoleMessage(
    ConsoleMessage* consoleMessage) {
  frame()->console().addMessage(consoleMessage);
}

void MainThreadWorkletGlobalScope::exceptionThrown(ErrorEvent* event) {
  MainThreadDebugger::instance()->exceptionThrown(this, event);
}

}  // namespace blink
