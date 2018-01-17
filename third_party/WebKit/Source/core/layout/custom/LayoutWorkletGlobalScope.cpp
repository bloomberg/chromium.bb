// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/custom/LayoutWorkletGlobalScope.h"

#include "bindings/core/v8/WorkerOrWorkletScriptController.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/inspector/MainThreadDebugger.h"
#include "core/origin_trials/OriginTrialContext.h"
#include "core/workers/GlobalScopeCreationParams.h"

namespace blink {

// static
LayoutWorkletGlobalScope* LayoutWorkletGlobalScope::Create(
    LocalFrame* frame,
    std::unique_ptr<GlobalScopeCreationParams> creation_params,
    WorkerReportingProxy& reporting_proxy,
    size_t global_scope_number) {
  auto* global_scope = new LayoutWorkletGlobalScope(
      frame, std::move(creation_params), reporting_proxy);
  String context_name("LayoutWorklet #");
  context_name.append(String::Number(global_scope_number));
  global_scope->ScriptController()->InitializeContextIfNeeded(context_name);
  MainThreadDebugger::Instance()->ContextCreated(
      global_scope->ScriptController()->GetScriptState(),
      global_scope->GetFrame(), global_scope->GetSecurityOrigin());
  return global_scope;
}

LayoutWorkletGlobalScope::LayoutWorkletGlobalScope(
    LocalFrame* frame,
    std::unique_ptr<GlobalScopeCreationParams> creation_params,
    WorkerReportingProxy& reporting_proxy)
    : MainThreadWorkletGlobalScope(frame,
                                   std::move(creation_params),
                                   reporting_proxy) {}

LayoutWorkletGlobalScope::~LayoutWorkletGlobalScope() = default;

void LayoutWorkletGlobalScope::Dispose() {
  MainThreadDebugger::Instance()->ContextWillBeDestroyed(
      ScriptController()->GetScriptState());

  MainThreadWorkletGlobalScope::Dispose();
}

}  // namespace blink
