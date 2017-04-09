// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/WorkletGlobalScope.h"

#include <memory>
#include "bindings/core/v8/SourceLocation.h"
#include "bindings/core/v8/WorkerOrWorkletScriptController.h"
#include "core/inspector/MainThreadDebugger.h"
#include "core/probe/CoreProbes.h"

namespace blink {

WorkletGlobalScope::WorkletGlobalScope(
    const KURL& url,
    const String& user_agent,
    PassRefPtr<SecurityOrigin> security_origin,
    v8::Isolate* isolate)
    : url_(url),
      user_agent_(user_agent),
      script_controller_(
          WorkerOrWorkletScriptController::Create(this, isolate)) {
  SetSecurityOrigin(std::move(security_origin));
}

WorkletGlobalScope::~WorkletGlobalScope() {}

void WorkletGlobalScope::Dispose() {
  DCHECK(script_controller_);
  script_controller_->Dispose();
  script_controller_.Clear();
}

v8::Local<v8::Object> WorkletGlobalScope::Wrap(
    v8::Isolate*,
    v8::Local<v8::Object> creation_context) {
  LOG(FATAL) << "WorkletGlobalScope must never be wrapped with wrap method. "
                "The global object of ECMAScript environment is used as the "
                "wrapper.";
  return v8::Local<v8::Object>();
}

v8::Local<v8::Object> WorkletGlobalScope::AssociateWithWrapper(
    v8::Isolate*,
    const WrapperTypeInfo*,
    v8::Local<v8::Object> wrapper) {
  LOG(FATAL) << "WorkletGlobalScope must never be wrapped with wrap method. "
                "The global object of ECMAScript environment is used as the "
                "wrapper.";
  return v8::Local<v8::Object>();
}

void WorkletGlobalScope::DisableEval(const String& error_message) {
  script_controller_->DisableEval(error_message);
}

bool WorkletGlobalScope::IsJSExecutionForbidden() const {
  return script_controller_->IsExecutionForbidden();
}

bool WorkletGlobalScope::IsSecureContext(
    String& error_message,
    const SecureContextCheck privilege_context_check) const {
  // Until there are APIs that are available in worklets and that
  // require a privileged context test that checks ancestors, just do
  // a simple check here.
  if (GetSecurityOrigin()->IsPotentiallyTrustworthy())
    return true;
  error_message = GetSecurityOrigin()->IsPotentiallyTrustworthyErrorMessage();
  return false;
}

KURL WorkletGlobalScope::VirtualCompleteURL(const String& url) const {
  // Always return a null URL when passed a null string.
  // TODO(ikilpatrick): Should we change the KURL constructor to have this
  // behavior?
  if (url.IsNull())
    return KURL();
  // Always use UTF-8 in Worklets.
  return KURL(url_, url);
}

DEFINE_TRACE(WorkletGlobalScope) {
  visitor->Trace(script_controller_);
  ExecutionContext::Trace(visitor);
  SecurityContext::Trace(visitor);
  WorkerOrWorkletGlobalScope::Trace(visitor);
}

}  // namespace blink
