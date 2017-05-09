// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WorkletGlobalScope_h
#define WorkletGlobalScope_h

#include <memory>
#include "core/CoreExport.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/SecurityContext.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/workers/WorkerOrWorkletGlobalScope.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"

namespace blink {

class EventQueue;

class CORE_EXPORT WorkletGlobalScope
    : public GarbageCollectedFinalized<WorkletGlobalScope>,
      public SecurityContext,
      public WorkerOrWorkletGlobalScope,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(WorkletGlobalScope);

 public:
  ~WorkletGlobalScope() override;

  bool IsWorkletGlobalScope() const final { return true; }

  // WorkerOrWorkletGlobalScope
  ScriptWrappable* GetScriptWrappable() const final {
    return const_cast<WorkletGlobalScope*>(this);
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
  String UserAgent() const final { return user_agent_; }
  SecurityContext& GetSecurityContext() final { return *this; }
  EventQueue* GetEventQueue() const final {
    NOTREACHED();
    return nullptr;
  }  // WorkletGlobalScopes don't have an event queue.
  bool IsSecureContext(String& error_message) const final;

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
};

DEFINE_TYPE_CASTS(WorkletGlobalScope,
                  ExecutionContext,
                  context,
                  context->IsWorkletGlobalScope(),
                  context.IsWorkletGlobalScope());

}  // namespace blink

#endif  // WorkletGlobalScope_h
