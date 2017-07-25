// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MainThreadWorkletGlobalScope_h
#define MainThreadWorkletGlobalScope_h

#include "core/CoreExport.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "core/dom/ExecutionContext.h"
#include "core/workers/WorkletGlobalScope.h"
#include "core/workers/WorkletModuleResponsesMapProxy.h"
#include "core/workers/WorkletPendingTasks.h"
#include "platform/WebTaskRunner.h"
#include "public/platform/WebURLRequest.h"

namespace blink {

class ConsoleMessage;
class LocalFrame;
class WorkletModuleResponsesMap;

class CORE_EXPORT MainThreadWorkletGlobalScope
    : public WorkletGlobalScope,
      public ContextClient {
  USING_GARBAGE_COLLECTED_MIXIN(MainThreadWorkletGlobalScope);

 public:
  MainThreadWorkletGlobalScope(LocalFrame*,
                               const KURL&,
                               const String& user_agent,
                               PassRefPtr<SecurityOrigin>,
                               v8::Isolate*);
  ~MainThreadWorkletGlobalScope() override;
  bool IsMainThreadWorkletGlobalScope() const final { return true; }

  // WorkerOrWorkletGlobalScope
  void ReportFeature(WebFeature) override;
  void ReportDeprecation(WebFeature) override;
  WorkerThread* GetThread() const final;

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

  void Terminate();

  WorkletModuleResponsesMapProxy* GetModuleResponsesMapProxy() const;

  // ExecutionContext
  void AddConsoleMessage(ConsoleMessage*) final;
  void ExceptionThrown(ErrorEvent*) final;
  CoreProbeSink* GetProbeSink() final;

  DECLARE_VIRTUAL_TRACE();

 private:
  Member<WorkletModuleResponsesMapProxy> module_responses_map_proxy_;
};

DEFINE_TYPE_CASTS(MainThreadWorkletGlobalScope,
                  ExecutionContext,
                  context,
                  context->IsMainThreadWorkletGlobalScope(),
                  context.IsMainThreadWorkletGlobalScope());

}  // namespace blink

#endif  // MainThreadWorkletGlobalScope_h
