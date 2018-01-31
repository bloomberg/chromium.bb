// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WorkletGlobalScope_h
#define WorkletGlobalScope_h

#include <memory>
#include "base/single_thread_task_runner.h"
#include "bindings/core/v8/ActiveScriptWrappable.h"
#include "core/CoreExport.h"
#include "core/dom/ExecutionContext.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/workers/WorkerOrWorkletGlobalScope.h"
#include "core/workers/WorkletModuleResponsesMapProxy.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/bindings/TraceWrapperMember.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebURLRequest.h"

namespace blink {

class WorkletModuleResponsesMap;
class WorkletPendingTasks;
class WorkerReportingProxy;
struct GlobalScopeCreationParams;

class CORE_EXPORT WorkletGlobalScope
    : public WorkerOrWorkletGlobalScope,
      public ActiveScriptWrappable<WorkletGlobalScope> {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(WorkletGlobalScope);

 public:
  ~WorkletGlobalScope() override;

  bool IsWorkletGlobalScope() const final { return true; }

  // Always returns false here as PaintWorkletGlobalScope and
  // AnimationWorkletGlobalScope don't have a #close() method on the global.
  // Note that AudioWorkletGlobal overrides this behavior.
  bool IsClosing() const { return false; }

  ExecutionContext* GetExecutionContext() const;

  // ExecutionContext
  const KURL& Url() const final { return url_; }
  const KURL& BaseURL() const final { return url_; }
  KURL CompleteURL(const String&) const final;
  String UserAgent() const final { return user_agent_; }
  SecurityContext& GetSecurityContext() final { return *this; }
  bool IsSecureContext(String& error_message) const final;

  DOMTimerCoordinator* Timers() final {
    // WorkletGlobalScopes don't have timers.
    NOTREACHED();
    return nullptr;
  }

  // Implementation of the "fetch and invoke a worklet script" algorithm:
  // https://drafts.css-houdini.org/worklets/#fetch-and-invoke-a-worklet-script
  // When script evaluation is done or any exception happens, it's notified to
  // the given WorkletPendingTasks via |outside_settings_task_runner| (i.e., the
  // parent frame's task runner).
  void FetchAndInvokeScript(
      const KURL& module_url_record,
      WorkletModuleResponsesMap*,
      network::mojom::FetchCredentialsMode,
      scoped_refptr<base::SingleThreadTaskRunner> outside_settings_task_runner,
      WorkletPendingTasks*);

  WorkletModuleResponsesMapProxy* ModuleResponsesMapProxy() const;
  void SetModuleResponsesMapProxyForTesting(WorkletModuleResponsesMapProxy*);

  const SecurityOrigin* DocumentSecurityOrigin() const {
    return document_security_origin_.get();
  }

  // Customize the security context used for origin trials.
  // Origin trials are only enabled in secure contexts, but WorkletGlobalScopes
  // are defined to have a unique, opaque origin, so are not secure:
  // https://drafts.css-houdini.org/worklets/#script-settings-for-worklets
  // For origin trials, instead consider the context of the document which
  // created the worklet, since the origin trial tokens are inherited from the
  // document.
  bool DocumentSecureContext() const { return document_secure_context_; }

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

  // The |url_| and |user_agent_| are inherited from the parent Document.
  const KURL url_;
  const String user_agent_;

  // Used for module fetch and origin trials, inherited from the parent
  // Document.
  const scoped_refptr<const SecurityOrigin> document_security_origin_;

  // Used for origin trials, inherited from the parent Document.
  const bool document_secure_context_;

  Member<WorkletModuleResponsesMapProxy> module_responses_map_proxy_;
};

DEFINE_TYPE_CASTS(WorkletGlobalScope,
                  ExecutionContext,
                  context,
                  context->IsWorkletGlobalScope(),
                  context.IsWorkletGlobalScope());

}  // namespace blink

#endif  // WorkletGlobalScope_h
