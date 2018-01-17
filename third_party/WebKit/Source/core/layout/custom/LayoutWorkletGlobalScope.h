// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayoutWorkletGlobalScope_h
#define LayoutWorkletGlobalScope_h

#include "bindings/core/v8/ScriptValue.h"
#include "core/CoreExport.h"
#include "core/dom/ExecutionContext.h"
#include "core/workers/MainThreadWorkletGlobalScope.h"
#include "platform/bindings/ScriptWrappable.h"

namespace blink {

class WorkerReportingProxy;

class CORE_EXPORT LayoutWorkletGlobalScope final
    : public MainThreadWorkletGlobalScope {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(LayoutWorkletGlobalScope);

 public:
  static LayoutWorkletGlobalScope* Create(
      LocalFrame*,
      std::unique_ptr<GlobalScopeCreationParams>,
      WorkerReportingProxy&,
      size_t global_scope_number);
  ~LayoutWorkletGlobalScope() override;
  void Dispose() final;

  bool IsLayoutWorkletGlobalScope() const final { return true; }

 private:
  LayoutWorkletGlobalScope(LocalFrame*,
                           std::unique_ptr<GlobalScopeCreationParams>,
                           WorkerReportingProxy&);
};

DEFINE_TYPE_CASTS(LayoutWorkletGlobalScope,
                  ExecutionContext,
                  context,
                  context->IsLayoutWorkletGlobalScope(),
                  context.IsLayoutWorkletGlobalScope());

}  // namespace blink

#endif  // LayoutWorkletGlobalScope_h
