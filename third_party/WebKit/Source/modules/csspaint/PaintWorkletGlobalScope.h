// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaintWorkletGlobalScope_h
#define PaintWorkletGlobalScope_h

#include "bindings/core/v8/ScriptValue.h"
#include "core/dom/ExecutionContext.h"
#include "core/workers/MainThreadWorkletGlobalScope.h"
#include "modules/ModulesExport.h"
#include "modules/csspaint/PaintWorkletPendingGeneratorRegistry.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/graphics/ImageBuffer.h"

namespace blink {

class CSSPaintDefinition;
class ExceptionState;
class WorkerReportingProxy;

class MODULES_EXPORT PaintWorkletGlobalScope final
    : public MainThreadWorkletGlobalScope {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(PaintWorkletGlobalScope);

 public:
  static PaintWorkletGlobalScope* Create(LocalFrame*,
                                         const KURL&,
                                         const String& user_agent,
                                         PassRefPtr<SecurityOrigin>,
                                         v8::Isolate*,
                                         WorkerReportingProxy&,
                                         PaintWorkletPendingGeneratorRegistry*,
                                         size_t global_scope_number);
  ~PaintWorkletGlobalScope() override;
  void Dispose() final;

  bool IsPaintWorkletGlobalScope() const final { return true; }
  void registerPaint(const String& name,
                     const ScriptValue& ctor,
                     ExceptionState&);

  CSSPaintDefinition* FindDefinition(const String& name);
  double devicePixelRatio() const;

  DECLARE_VIRTUAL_TRACE();
  DECLARE_TRACE_WRAPPERS();

 private:
  PaintWorkletGlobalScope(LocalFrame*,
                          const KURL&,
                          const String& user_agent,
                          PassRefPtr<SecurityOrigin>,
                          v8::Isolate*,
                          WorkerReportingProxy&,
                          PaintWorkletPendingGeneratorRegistry*);

  // The implementation of the "paint definition" concept:
  // https://drafts.css-houdini.org/css-paint-api/#paint-definition
  typedef HeapHashMap<String, TraceWrapperMember<CSSPaintDefinition>>
      DefinitionMap;
  DefinitionMap paint_definitions_;

  Member<PaintWorkletPendingGeneratorRegistry> pending_generator_registry_;
};

DEFINE_TYPE_CASTS(PaintWorkletGlobalScope,
                  ExecutionContext,
                  context,
                  context->IsPaintWorkletGlobalScope(),
                  context.IsPaintWorkletGlobalScope());

}  // namespace blink

#endif  // PaintWorkletGlobalScope_h
