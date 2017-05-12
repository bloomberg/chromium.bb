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
    : public WorkletGlobalScopeProxy {
 public:
  static PaintWorkletGlobalScopeProxy* From(WorkletGlobalScopeProxy*);

  explicit PaintWorkletGlobalScopeProxy(LocalFrame*);
  virtual ~PaintWorkletGlobalScopeProxy() = default;

  // Implements WorkletGlobalScopeProxy.
  void FetchAndInvokeScript(const KURL& module_url_record,
                            WebURLRequest::FetchCredentialsMode,
                            WorkletPendingTasks*) override;
  void EvaluateScript(const ScriptSourceCode&) override;
  void TerminateWorkletGlobalScope() override;

  CSSPaintDefinition* FindDefinition(const String& name);
  void AddPendingGenerator(const String& name, CSSPaintImageGeneratorImpl*);

  PaintWorkletGlobalScope* global_scope() const { return global_scope_.Get(); }

 private:
  Persistent<PaintWorkletGlobalScope> global_scope_;
};

}  // namespace blink

#endif  // PaintWorkletGlobalScopeProxy_h
