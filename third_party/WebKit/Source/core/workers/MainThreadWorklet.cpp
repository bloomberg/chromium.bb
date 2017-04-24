// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/MainThreadWorklet.h"

#include "bindings/core/v8/ScriptSourceCode.h"
#include "bindings/core/v8/V8Binding.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/frame/LocalFrame.h"
#include "core/workers/WorkletGlobalScopeProxy.h"
#include "platform/wtf/WTF.h"

namespace blink {

namespace {

int32_t GetNextRequestId() {
  DCHECK(IsMainThread());
  static int32_t next_request_id = 1;
  CHECK_LT(next_request_id, std::numeric_limits<int32_t>::max());
  return next_request_id++;
}

}  // namespace

MainThreadWorklet::MainThreadWorklet(LocalFrame* frame) : Worklet(frame) {
  DCHECK(resolver_map_.IsEmpty());
}

ScriptPromise MainThreadWorklet::addModule(ScriptState* script_state,
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

  int32_t request_id = GetNextRequestId();
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();
  resolver_map_.Set(request_id, resolver);
  GetWorkletGlobalScopeProxy()->FetchAndInvokeScript(request_id, script_url);
  return promise;
}

void MainThreadWorklet::DidFetchAndInvokeScript(int32_t request_id,
                                                bool success) {
  DCHECK(IsMainThread());
  ScriptPromiseResolver* resolver = resolver_map_.at(request_id);
  if (!resolver)
    return;
  resolver_map_.erase(request_id);
  if (!success) {
    resolver->Reject(DOMException::Create(kNetworkError));
    return;
  }
  resolver->Resolve();
}

void MainThreadWorklet::ContextDestroyed(ExecutionContext* execution_context) {
  DCHECK(IsMainThread());
  resolver_map_.clear();
  Worklet::ContextDestroyed(execution_context);
}

DEFINE_TRACE(MainThreadWorklet) {
  visitor->Trace(resolver_map_);
  Worklet::Trace(visitor);
}

}  // namespace blink
