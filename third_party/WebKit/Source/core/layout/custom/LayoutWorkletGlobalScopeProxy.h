// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayoutWorkletGlobalScopeProxy_h
#define LayoutWorkletGlobalScopeProxy_h

#include "core/CoreExport.h"
#include "core/layout/custom/LayoutWorkletGlobalScope.h"
#include "core/workers/MainThreadWorkletReportingProxy.h"
#include "core/workers/WorkletGlobalScopeProxy.h"

namespace blink {

class LocalFrame;

// A proxy for LayoutWorklet to talk to LayoutWorkletGlobalScope.
class CORE_EXPORT LayoutWorkletGlobalScopeProxy
    : public GarbageCollectedFinalized<LayoutWorkletGlobalScopeProxy>,
      public WorkletGlobalScopeProxy {
  USING_GARBAGE_COLLECTED_MIXIN(LayoutWorkletGlobalScopeProxy);

 public:
  LayoutWorkletGlobalScopeProxy(LocalFrame*, size_t global_scope_number);
  ~LayoutWorkletGlobalScopeProxy() override = default;

  // Implements WorkletGlobalScopeProxy.
  void FetchAndInvokeScript(
      const KURL& module_url_record,
      WorkletModuleResponsesMap*,
      network::mojom::FetchCredentialsMode,
      scoped_refptr<WebTaskRunner> outside_settings_task_runner,
      WorkletPendingTasks*) override;
  void WorkletObjectDestroyed() override;
  void TerminateWorkletGlobalScope() override;

  void Trace(blink::Visitor*) override;

 private:
  std::unique_ptr<MainThreadWorkletReportingProxy> reporting_proxy_;
  Member<LayoutWorkletGlobalScope> global_scope_;
};

}  // namespace blink

#endif  // LayoutWorkletGlobalScopeProxy_h
