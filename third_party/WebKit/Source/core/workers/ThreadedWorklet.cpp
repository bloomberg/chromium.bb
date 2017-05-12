// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/ThreadedWorklet.h"

#include "bindings/core/v8/ScriptSourceCode.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/frame/LocalFrame.h"
#include "core/workers/WorkletGlobalScopeProxy.h"
#include "platform/wtf/WTF.h"

namespace blink {

ThreadedWorklet::ThreadedWorklet(LocalFrame* frame)
    : Worklet(frame), frame_(frame) {}

void ThreadedWorklet::FetchAndInvokeScript(const KURL& module_url_record,
                                           const WorkletOptions&,
                                           ScriptPromiseResolver* resolver) {
  DCHECK(IsMainThread());
  if (!GetExecutionContext())
    return;

  if (!IsInitialized())
    Initialize();

  WorkletScriptLoader* script_loader =
      WorkletScriptLoader::Create(frame_->GetDocument()->Fetcher(), this);
  loader_to_resolver_map_.Set(script_loader, resolver);
  script_loader->FetchScript(module_url_record);
}

void ThreadedWorklet::NotifyWorkletScriptLoadingFinished(
    WorkletScriptLoader* script_loader,
    const ScriptSourceCode& source_code) {
  DCHECK(IsMainThread());
  ScriptPromiseResolver* resolver = loader_to_resolver_map_.at(script_loader);
  loader_to_resolver_map_.erase(script_loader);

  if (!script_loader->WasScriptLoadSuccessful()) {
    resolver->Reject(DOMException::Create(kNetworkError));
    return;
  }

  GetWorkletGlobalScopeProxy()->EvaluateScript(source_code);
  resolver->Resolve();
}

void ThreadedWorklet::ContextDestroyed(ExecutionContext* execution_context) {
  DCHECK(IsMainThread());
  for (const auto& script_loader : loader_to_resolver_map_.Keys())
    script_loader->Cancel();
  loader_to_resolver_map_.clear();
  if (IsInitialized())
    GetWorkletGlobalScopeProxy()->TerminateWorkletGlobalScope();
  frame_ = nullptr;
}

DEFINE_TRACE(ThreadedWorklet) {
  visitor->Trace(frame_);
  visitor->Trace(loader_to_resolver_map_);
  Worklet::Trace(visitor);
}

}  // namespace blink
