// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WorkletGlobalScope_h
#define WorkletGlobalScope_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/CoreExport.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/SecurityContext.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/workers/WorkerOrWorkletGlobalScope.h"
#include "platform/heap/Handle.h"
#include <memory>

namespace blink {

class EventQueue;
class WorkerOrWorkletScriptController;

class CORE_EXPORT WorkletGlobalScope
    : public GarbageCollectedFinalized<WorkletGlobalScope>,
      public SecurityContext,
      public WorkerOrWorkletGlobalScope,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(WorkletGlobalScope);

 public:
  ~WorkletGlobalScope() override;
  void Dispose() override;

  bool IsWorkletGlobalScope() const final { return true; }

  // WorkerOrWorkletGlobalScope
  ScriptWrappable* GetScriptWrappable() const final {
    return const_cast<WorkletGlobalScope*>(this);
  }
  WorkerOrWorkletScriptController* ScriptController() final {
    return script_controller_.Get();
  }

  // Always returns false here as worklets don't have a #close() method on
  // the global.
  bool IsClosing() const final { return false; }

  // ScriptWrappable
  v8::Local<v8::Object> Wrap(v8::Isolate*,
                             v8::Local<v8::Object> creation_context) final;
  v8::Local<v8::Object> AssociateWithWrapper(
      v8::Isolate*,
      const WrapperTypeInfo*,
      v8::Local<v8::Object> wrapper) final;

  // ExecutionContext
  void DisableEval(const String& error_message) final;
  bool IsJSExecutionForbidden() const final;
  String UserAgent() const final { return user_agent_; }
  SecurityContext& GetSecurityContext() final { return *this; }
  EventQueue* GetEventQueue() const final {
    NOTREACHED();
    return nullptr;
  }  // WorkletGlobalScopes don't have an event queue.
  bool IsSecureContext(
      String& error_message,
      const SecureContextCheck = kStandardSecureContextCheck) const final;

  using SecurityContext::GetSecurityOrigin;
  using SecurityContext::GetContentSecurityPolicy;

  DOMTimerCoordinator* Timers() final {
    NOTREACHED();
    return nullptr;
  }  // WorkletGlobalScopes don't have timers.

  DECLARE_VIRTUAL_TRACE();

 protected:
  // The url, userAgent and securityOrigin arguments are inherited from the
  // parent ExecutionContext for Worklets.
  WorkletGlobalScope(const KURL&,
                     const String& user_agent,
                     PassRefPtr<SecurityOrigin>,
                     v8::Isolate*);

 private:
  const KURL& VirtualURL() const final { return url_; }
  KURL VirtualCompleteURL(const String&) const final;

  EventTarget* ErrorEventTarget() final { return nullptr; }
  void DidUpdateSecurityOrigin() final {}

  KURL url_;
  String user_agent_;
  Member<WorkerOrWorkletScriptController> script_controller_;
};

DEFINE_TYPE_CASTS(WorkletGlobalScope,
                  ExecutionContext,
                  context,
                  context->IsWorkletGlobalScope(),
                  context.IsWorkletGlobalScope());

}  // namespace blink

#endif  // WorkletGlobalScope_h
