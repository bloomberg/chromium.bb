// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaintWorkletGlobalScopeProxy_h
#define PaintWorkletGlobalScopeProxy_h

#include "core/workers/WorkletGlobalScopeProxy.h"

#include "modules/ModulesExport.h"
#include "modules/csspaint/PaintWorkletGlobalScope.h"

namespace blink {

class CSSPaintDefinition;
class LocalFrame;

// A proxy for PaintWorklet to talk to PaintWorkletGlobalScope.
class MODULES_EXPORT PaintWorkletGlobalScopeProxy
    : public GarbageCollectedFinalized<PaintWorkletGlobalScopeProxy>,
      public WorkletGlobalScopeProxy {
  USING_GARBAGE_COLLECTED_MIXIN(PaintWorkletGlobalScopeProxy);

 public:
  static PaintWorkletGlobalScopeProxy* From(WorkletGlobalScopeProxy*);

  PaintWorkletGlobalScopeProxy(LocalFrame*,
                               PaintWorkletPendingGeneratorRegistry*,
                               size_t global_scope_number);
  virtual ~PaintWorkletGlobalScopeProxy() = default;

  // Implements WorkletGlobalScopeProxy.
  void FetchAndInvokeScript(const KURL& module_url_record,
                            WorkletModuleResponsesMap*,
                            WebURLRequest::FetchCredentialsMode,
                            RefPtr<WebTaskRunner> outside_settings_task_runner,
                            WorkletPendingTasks*) override;
  void WorkletObjectDestroyed() override;
  void TerminateWorkletGlobalScope() override;

  CSSPaintDefinition* FindDefinition(const String& name);

  PaintWorkletGlobalScope* global_scope() const { return global_scope_.Get(); }

  DECLARE_VIRTUAL_TRACE();

 private:
  Member<PaintWorkletGlobalScope> global_scope_;
};

}  // namespace blink

#endif  // PaintWorkletGlobalScopeProxy_h
