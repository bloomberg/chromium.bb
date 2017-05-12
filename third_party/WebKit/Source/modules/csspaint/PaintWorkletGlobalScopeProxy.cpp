// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/csspaint/PaintWorkletGlobalScopeProxy.h"

#include "bindings/core/v8/ScriptSourceCode.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "core/dom/Document.h"
#include "core/frame/LocalFrame.h"
#include "platform/weborigin/KURL.h"
#include "platform/wtf/WTF.h"

namespace blink {

PaintWorkletGlobalScopeProxy* PaintWorkletGlobalScopeProxy::From(
    WorkletGlobalScopeProxy* proxy) {
  return static_cast<PaintWorkletGlobalScopeProxy*>(proxy);
}

PaintWorkletGlobalScopeProxy::PaintWorkletGlobalScopeProxy(LocalFrame* frame) {
  DCHECK(IsMainThread());
  Document* document = frame->GetDocument();
  global_scope_ = PaintWorkletGlobalScope::Create(
      frame, document->Url(), document->UserAgent(),
      document->GetSecurityOrigin(), ToIsolate(document));
}

void PaintWorkletGlobalScopeProxy::FetchAndInvokeScript(
    const KURL& module_url_record,
    WebURLRequest::FetchCredentialsMode credentials_mode,
    WorkletPendingTasks* pending_tasks) {
  DCHECK(IsMainThread());
  global_scope_->FetchAndInvokeScript(module_url_record, credentials_mode,
                                      pending_tasks);
}

void PaintWorkletGlobalScopeProxy::EvaluateScript(
    const ScriptSourceCode& script_source_code) {
  // This should be called only for threaded worklets that still use classic
  // script loading.
  NOTREACHED();
}

void PaintWorkletGlobalScopeProxy::TerminateWorkletGlobalScope() {
  DCHECK(IsMainThread());
  global_scope_->Terminate();
  // Nullify the global scope to cut a potential reference cycle.
  global_scope_ = nullptr;
}

CSSPaintDefinition* PaintWorkletGlobalScopeProxy::FindDefinition(
    const String& name) {
  DCHECK(IsMainThread());
  return global_scope_->FindDefinition(name);
}

void PaintWorkletGlobalScopeProxy::AddPendingGenerator(
    const String& name,
    CSSPaintImageGeneratorImpl* generator) {
  DCHECK(IsMainThread());
  global_scope_->AddPendingGenerator(name, generator);
}

}  // namespace blink
