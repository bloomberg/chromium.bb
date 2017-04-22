// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/MainThreadWorkletGlobalScope.h"

#include "bindings/core/v8/ScriptSourceCode.h"
#include "bindings/core/v8/WorkerOrWorkletScriptController.h"
#include "core/dom/Document.h"
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
    v8::Isolate* isolate,
    WorkletObjectProxy* object_proxy)
    : WorkletGlobalScope(url, user_agent, std::move(security_origin), isolate),
      ContextClient(frame),
      object_proxy_(object_proxy) {}

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

void MainThreadWorkletGlobalScope::FetchAndInvokeScript(
    int32_t request_id,
    const KURL& script_url) {
  DCHECK(IsMainThread());
  // TODO(nhiroki): Replace this with module script loading.
  WorkletScriptLoader* script_loader =
      WorkletScriptLoader::Create(GetFrame()->GetDocument()->Fetcher(), this);
  script_loader->set_request_id(request_id);
  loader_set_.insert(script_loader);
  script_loader->FetchScript(script_url);
}

void MainThreadWorkletGlobalScope::EvaluateScript(
    const ScriptSourceCode& script_source_code) {
  // This should be called only for threaded worklets that still use classic
  // script loading.
  NOTREACHED();
}

void MainThreadWorkletGlobalScope::TerminateWorkletGlobalScope() {
  for (const auto& script_loader : loader_set_)
    script_loader->Cancel();
  Dispose();
}

void MainThreadWorkletGlobalScope::NotifyWorkletScriptLoadingFinished(
    WorkletScriptLoader* script_loader,
    const ScriptSourceCode& source_code) {
  DCHECK(IsMainThread());
  int32_t request_id = script_loader->request_id();
  loader_set_.erase(script_loader);
  bool success = script_loader->WasScriptLoadSuccessful();
  if (success)
    ScriptController()->Evaluate(source_code);
  object_proxy_->DidFetchAndInvokeScript(request_id, success);
}

void MainThreadWorkletGlobalScope::AddConsoleMessage(
    ConsoleMessage* console_message) {
  GetFrame()->Console().AddMessage(console_message);
}

void MainThreadWorkletGlobalScope::ExceptionThrown(ErrorEvent* event) {
  MainThreadDebugger::Instance()->ExceptionThrown(this, event);
}

DEFINE_TRACE(MainThreadWorkletGlobalScope) {
  visitor->Trace(loader_set_);
  visitor->Trace(object_proxy_);
  WorkletGlobalScope::Trace(visitor);
  ContextClient::Trace(visitor);
}

}  // namespace blink
