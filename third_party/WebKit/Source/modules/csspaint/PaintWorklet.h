// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaintWorklet_h
#define PaintWorklet_h

#include "core/workers/MainThreadWorklet.h"
#include "modules/ModulesExport.h"
#include "modules/csspaint/PaintWorkletGlobalScopeProxy.h"
#include "platform/heap/Handle.h"

namespace blink {

class CSSPaintDefinition;
class CSSPaintImageGeneratorImpl;

// Manages a paint worklet:
// https://drafts.css-houdini.org/css-paint-api/#dom-css-paintworklet
class MODULES_EXPORT PaintWorklet final : public MainThreadWorklet {
  WTF_MAKE_NONCOPYABLE(PaintWorklet);

 public:
  static PaintWorklet* Create(LocalFrame*);
  ~PaintWorklet() override;

  WorkletGlobalScopeProxy* GetWorkletGlobalScopeProxy() const final;
  CSSPaintDefinition* FindDefinition(const String& name);
  void AddPendingGenerator(const String& name, CSSPaintImageGeneratorImpl*);

  DECLARE_VIRTUAL_TRACE();

 private:
  explicit PaintWorklet(LocalFrame*);

  // TODO(nhiroki): Make (Paint)WorkletGlobalScopeProxy GC-managed object.
  std::unique_ptr<PaintWorkletGlobalScopeProxy> global_scope_proxy_;
};

}  // namespace blink

#endif  // PaintWorklet_h
