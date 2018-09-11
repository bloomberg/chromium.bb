// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_WORKLET_GLOBAL_SCOPE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_WORKLET_GLOBAL_SCOPE_H_

#include <memory>
#include "base/single_thread_task_runner.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/workers/worker_or_worklet_global_scope.h"
#include "third_party/blink/renderer/core/workers/worklet_module_responses_map.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/bindings/trace_wrapper_member.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class FetchClientSettingsObjectSnapshot;
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
  bool IsClosing() const override { return false; }

  ExecutionContext* GetExecutionContext() const override;

  // ExecutionContext
  const KURL& Url() const final { return url_; }
  const KURL& BaseURL() const final { return url_; }
  KURL CompleteURL(const String&) const final;
  String UserAgent() const final { return user_agent_; }
  SecurityContext& GetSecurityContext() final { return *this; }
  bool IsSecureContext(String& error_message) const final;

  // Currently, worklet agents have no clearly defined owner. See
  // https://html.spec.whatwg.org/multipage/webappapis.html#integration-with-the-javascript-agent-cluster-formalism
  const base::UnguessableToken& GetAgentClusterID() const final {
    return base::UnguessableToken::Null();
  }

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
      network::mojom::FetchCredentialsMode,
      FetchClientSettingsObjectSnapshot* outside_settings_object,
      scoped_refptr<base::SingleThreadTaskRunner> outside_settings_task_runner,
      WorkletPendingTasks*);

  WorkletModuleResponsesMap* GetModuleResponsesMap() const {
    return module_responses_map_.Get();
  }

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

  HttpsState GetHttpsState() const override { return https_state_; }

 protected:
  // Partial implementation of the "set up a worklet environment settings
  // object" algorithm:
  // https://drafts.css-houdini.org/worklets/#script-settings-for-worklets
  WorkletGlobalScope(
      std::unique_ptr<GlobalScopeCreationParams>,
      v8::Isolate*,
      WorkerReportingProxy&,
      scoped_refptr<base::SingleThreadTaskRunner> document_loading_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> worklet_loading_task_runner);

  void BindContentSecurityPolicyToExecutionContext() override;

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

  CrossThreadPersistent<WorkletModuleResponsesMap> module_responses_map_;

  const HttpsState https_state_;
};

DEFINE_TYPE_CASTS(WorkletGlobalScope,
                  ExecutionContext,
                  context,
                  context->IsWorkletGlobalScope(),
                  context.IsWorkletGlobalScope());

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_WORKLET_GLOBAL_SCOPE_H_
