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

ThreadedWorklet::ThreadedWorklet(LocalFrame* frame) : Worklet(frame) {}

ScriptPromise ThreadedWorklet::addModule(ScriptState* script_state,
                                         const String& url) {
  DCHECK(IsMainThread());
  if (!GetExecutionContext()) {
    return ScriptPromise::RejectWithDOMException(
        script_state, DOMException::Create(kInvalidStateError,
                                           "This frame is already detached"));
  }

  KURL script_url = GetExecutionContext()->CompleteURL(url);
  if (!script_url.IsValid()) {
    return ScriptPromise::RejectWithDOMException(
        script_state, DOMException::Create(
                          kSyntaxError, "'" + url + "' is not a valid URL."));
  }

  if (!IsInitialized())
    Initialize();

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  WorkletScriptLoader* script_loader =
      WorkletScriptLoader::Create(frame_->GetDocument()->Fetcher(), this);
  loader_to_resolver_map_.Set(script_loader, resolver);
  script_loader->FetchScript(script_url);
  return promise;
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
  Worklet::ContextDestroyed(execution_context);
}

DEFINE_TRACE(ThreadedWorklet) {
  visitor->Trace(loader_to_resolver_map_);
  Worklet::Trace(visitor);
}

}  // namespace blink
