// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaintWorklet_h
#define PaintWorklet_h

#include "core/workers/Worklet.h"
#include "modules/ModulesExport.h"
#include "modules/csspaint/PaintWorkletGlobalScope.h"
#include "platform/heap/Handle.h"

namespace blink {

class CSSPaintDefinition;
class CSSPaintImageGeneratorImpl;

class MODULES_EXPORT PaintWorklet final : public Worklet {
  WTF_MAKE_NONCOPYABLE(PaintWorklet);

 public:
  static PaintWorklet* Create(LocalFrame*);
  ~PaintWorklet() override;

  PaintWorkletGlobalScope* GetWorkletGlobalScopeProxy() const final;
  CSSPaintDefinition* FindDefinition(const String& name);
  void AddPendingGenerator(const String& name, CSSPaintImageGeneratorImpl*);

  DECLARE_VIRTUAL_TRACE();

 private:
  explicit PaintWorklet(LocalFrame*);

  Member<PaintWorkletGlobalScope> paint_worklet_global_scope_;
};

}  // namespace blink

#endif  // PaintWorklet_h
