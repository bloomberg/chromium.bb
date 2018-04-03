// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayoutWorkletGlobalScope_h
#define LayoutWorkletGlobalScope_h

#include "bindings/core/v8/ScriptValue.h"
#include "core/CoreExport.h"
#include "core/execution_context/ExecutionContext.h"
#include "core/layout/custom/PendingLayoutRegistry.h"
#include "core/workers/MainThreadWorkletGlobalScope.h"
#include "platform/bindings/ScriptWrappable.h"

namespace blink {

class CSSLayoutDefinition;
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
      PendingLayoutRegistry*,
      size_t global_scope_number);
  ~LayoutWorkletGlobalScope() override;
  void Dispose() final;

  bool IsLayoutWorkletGlobalScope() const final { return true; }

  // Implements LayoutWorkletGlobalScope.idl
  void registerLayout(const AtomicString& name,
                      const ScriptValue& constructor_value,
                      ExceptionState&);

  CSSLayoutDefinition* FindDefinition(const AtomicString& name);

  void Trace(blink::Visitor*) override;
  void TraceWrappers(const ScriptWrappableVisitor*) const override;

 private:
  LayoutWorkletGlobalScope(LocalFrame*,
                           std::unique_ptr<GlobalScopeCreationParams>,
                           WorkerReportingProxy&,
                           PendingLayoutRegistry*);

  // https://drafts.css-houdini.org/css-layout-api/#layout-definitions
  typedef HeapHashMap<String, TraceWrapperMember<CSSLayoutDefinition>>
      DefinitionMap;
  DefinitionMap layout_definitions_;
  Member<PendingLayoutRegistry> pending_layout_registry_;
};

DEFINE_TYPE_CASTS(LayoutWorkletGlobalScope,
                  ExecutionContext,
                  context,
                  context->IsLayoutWorkletGlobalScope(),
                  context.IsLayoutWorkletGlobalScope());

}  // namespace blink

#endif  // LayoutWorkletGlobalScope_h
