// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/Worklet.h"

#include "bindings/core/v8/ScriptSourceCode.h"
#include "bindings/core/v8/V8Binding.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/frame/LocalFrame.h"
#include "core/workers/WorkletGlobalScopeProxy.h"
#include "wtf/WTF.h"

namespace blink {

Worklet::Worklet(LocalFrame* frame)
    : ContextLifecycleObserver(frame->document()), m_frame(frame) {}

ScriptPromise Worklet::import(ScriptState* scriptState, const String& url) {
  DCHECK(isMainThread());
  if (!getExecutionContext()) {
    return ScriptPromise::rejectWithDOMException(
        scriptState, DOMException::create(InvalidStateError,
                                          "This frame is already detached"));
  }

  KURL scriptURL = getExecutionContext()->completeURL(url);
  if (!scriptURL.isValid()) {
    return ScriptPromise::rejectWithDOMException(
        scriptState,
        DOMException::create(SyntaxError, "'" + url + "' is not a valid URL."));
  }

  if (!isInitialized())
    initialize();

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  ScriptPromise promise = resolver->promise();

  WorkletScriptLoader* scriptLoader =
      WorkletScriptLoader::create(m_frame->document()->fetcher(), this);
  m_loaderAndResolvers.set(scriptLoader, resolver);
  scriptLoader->fetchScript(scriptURL);
  return promise;
}

void Worklet::notifyWorkletScriptLoadingFinished(
    WorkletScriptLoader* scriptLoader,
    const ScriptSourceCode& sourceCode) {
  DCHECK(isMainThread());
  ScriptPromiseResolver* resolver = m_loaderAndResolvers.at(scriptLoader);
  m_loaderAndResolvers.erase(scriptLoader);

  if (!scriptLoader->wasScriptLoadSuccessful()) {
    resolver->reject(DOMException::create(NetworkError));
    return;
  }

  workletGlobalScopeProxy()->evaluateScript(sourceCode);
  resolver->resolve();
}

void Worklet::contextDestroyed(ExecutionContext*) {
  DCHECK(isMainThread());
  if (isInitialized())
    workletGlobalScopeProxy()->terminateWorkletGlobalScope();
  for (const auto& scriptLoader : m_loaderAndResolvers.keys())
    scriptLoader->cancel();
  m_loaderAndResolvers.clear();
  m_frame = nullptr;
}

DEFINE_TRACE(Worklet) {
  visitor->trace(m_frame);
  visitor->trace(m_loaderAndResolvers);
  ContextLifecycleObserver::trace(visitor);
}

}  // namespace blink
