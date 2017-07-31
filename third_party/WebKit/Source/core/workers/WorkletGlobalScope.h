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
#include "core/workers/WorkletModuleResponsesMapProxy.h"
#include "platform/WebTaskRunner.h"
#include "platform/bindings/ActiveScriptWrappable.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/bindings/TraceWrapperMember.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebURLRequest.h"

namespace blink {

class EventQueue;
class Modulator;
class WorkletModuleResponsesMap;
class WorkletPendingTasks;

class CORE_EXPORT WorkletGlobalScope
    : public GarbageCollectedFinalized<WorkletGlobalScope>,
      public SecurityContext,
      public WorkerOrWorkletGlobalScope,
      public ScriptWrappable,
      public ActiveScriptWrappable<WorkletGlobalScope> {
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
  bool HasPendingActivity() const override;

  ExecutionContext* GetExecutionContext() const;

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

  // Implementation of the "fetch and invoke a worklet script" algorithm:
  // https://drafts.css-houdini.org/worklets/#fetch-and-invoke-a-worklet-script
  // When script evaluation is done or any exception happens, it's notified to
  // the given WorkletPendingTasks via |outside_settings_task_runner| (i.e., the
  // parent frame's task runner).
  void FetchAndInvokeScript(const KURL& module_url_record,
                            WorkletModuleResponsesMap*,
                            WebURLRequest::FetchCredentialsMode,
                            RefPtr<WebTaskRunner> outside_settings_task_runner,
                            WorkletPendingTasks*);

  WorkletModuleResponsesMapProxy* ModuleResponsesMapProxy() const;
  void SetModuleResponsesMapProxyForTesting(WorkletModuleResponsesMapProxy*);

  void SetModulator(Modulator*);

  DECLARE_VIRTUAL_TRACE();
  DECLARE_VIRTUAL_TRACE_WRAPPERS();

 protected:
  // The url, userAgent and securityOrigin arguments are inherited from the
  // parent ExecutionContext for Worklets.
  WorkletGlobalScope(const KURL&,
                     const String& user_agent,
                     RefPtr<SecurityOrigin>,
                     v8::Isolate*,
                     WorkerClients*);

 private:
  const KURL& VirtualURL() const final { return url_; }
  KURL VirtualCompleteURL(const String&) const final;

  EventTarget* ErrorEventTarget() final { return nullptr; }
  void DidUpdateSecurityOrigin() final {}

  const KURL url_;
  const String user_agent_;
  Member<WorkletModuleResponsesMapProxy> module_responses_map_proxy_;
  // LocalDOMWindow::modulator_ workaround equivalent.
  // TODO(kouhei): Remove this.
  TraceWrapperMember<Modulator> modulator_;
};

DEFINE_TYPE_CASTS(WorkletGlobalScope,
                  ExecutionContext,
                  context,
                  context->IsWorkletGlobalScope(),
                  context.IsWorkletGlobalScope());

}  // namespace blink

#endif  // WorkletGlobalScope_h
