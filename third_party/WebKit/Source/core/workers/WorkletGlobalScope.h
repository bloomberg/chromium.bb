// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WorkletGlobalScope_h
#define WorkletGlobalScope_h

#include <memory>
#include "bindings/core/v8/ActiveScriptWrappable.h"
#include "core/CoreExport.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/SecurityContext.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/workers/WorkerOrWorkletGlobalScope.h"
#include "core/workers/WorkletModuleResponsesMapProxy.h"
#include "platform/WebTaskRunner.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/bindings/TraceWrapperMember.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebURLRequest.h"

namespace blink {

class Modulator;
class WorkletModuleResponsesMap;
class WorkletPendingTasks;
class WorkerReportingProxy;
struct GlobalScopeCreationParams;

class CORE_EXPORT WorkletGlobalScope
    : public WorkerOrWorkletGlobalScope,
      public SecurityContext,
      public ActiveScriptWrappable<WorkletGlobalScope> {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(WorkletGlobalScope);

 public:
  ~WorkletGlobalScope() override;

  bool IsWorkletGlobalScope() const final { return true; }

  void EvaluateClassicScript(
      const KURL& script_url,
      String source_code,
      std::unique_ptr<Vector<char>> cached_meta_data) final;

  // Always returns false here as worklets don't have a #close() method on
  // the global.
  bool IsClosing() const final { return false; }

  ExecutionContext* GetExecutionContext() const;

  // ExecutionContext
  const KURL& Url() const final { return url_; }
  const KURL& BaseURL() const final { return url_; }
  KURL CompleteURL(const String&) const final;
  String UserAgent() const final { return user_agent_; }
  SecurityContext& GetSecurityContext() final { return *this; }
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
  void FetchAndInvokeScript(
      const KURL& module_url_record,
      WorkletModuleResponsesMap*,
      network::mojom::FetchCredentialsMode,
      scoped_refptr<WebTaskRunner> outside_settings_task_runner,
      WorkletPendingTasks*);

  WorkletModuleResponsesMapProxy* ModuleResponsesMapProxy() const;
  void SetModuleResponsesMapProxyForTesting(WorkletModuleResponsesMapProxy*);

  void SetModulator(Modulator*);

  SecurityOrigin* DocumentSecurityOrigin() const {
    return document_security_origin_.get();
  }

  void Trace(blink::Visitor*) override;
  void TraceWrappers(const ScriptWrappableVisitor*) const override;

 protected:
  // Partial implementation of the "set up a worklet environment settings
  // object" algorithm:
  // https://drafts.css-houdini.org/worklets/#script-settings-for-worklets
  WorkletGlobalScope(std::unique_ptr<GlobalScopeCreationParams>,
                     v8::Isolate*,
                     WorkerReportingProxy&);

 private:
  EventTarget* ErrorEventTarget() final { return nullptr; }
  void DidUpdateSecurityOrigin() final {}

  // The |url_| and |user_agent_| are inherited from the parent Document.
  const KURL url_;
  const String user_agent_;

  // Used for module fetch.
  const scoped_refptr<SecurityOrigin> document_security_origin_;

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
