// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaintWorklet_h
#define PaintWorklet_h

#include "core/workers/Worklet.h"
#include "modules/ModulesExport.h"
#include "modules/csspaint/PaintWorkletGlobalScopeProxy.h"
#include "modules/csspaint/PaintWorkletPendingGeneratorRegistry.h"
#include "platform/heap/Handle.h"

namespace blink {

class CSSPaintDefinition;
class CSSPaintImageGeneratorImpl;

// Manages a paint worklet:
// https://drafts.css-houdini.org/css-paint-api/#dom-css-paintworklet
class MODULES_EXPORT PaintWorklet final : public Worklet {
  WTF_MAKE_NONCOPYABLE(PaintWorklet);

 public:
  static PaintWorklet* Create(LocalFrame*);
  ~PaintWorklet() override;

  CSSPaintDefinition* FindDefinition(const String& name);
  void AddPendingGenerator(const String& name, CSSPaintImageGeneratorImpl*);

  DECLARE_VIRTUAL_TRACE();

 private:
  friend class PaintWorkletTest;

  explicit PaintWorklet(LocalFrame*);

  // Implements Worklet.
  bool NeedsToCreateGlobalScope() final;
  WorkletGlobalScopeProxy* CreateGlobalScope() final;

  Member<PaintWorkletPendingGeneratorRegistry> pending_generator_registry_;

  // TODO(style-dev): Implement the "document paint definition" concept:
  // https://drafts.css-houdini.org/css-paint-api/#document-paint-definition
  // (https://crbug.com/578252)
};

}  // namespace blink

#endif  // PaintWorklet_h
