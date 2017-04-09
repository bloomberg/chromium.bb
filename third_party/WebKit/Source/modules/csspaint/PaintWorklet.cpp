// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/csspaint/PaintWorklet.h"

#include "bindings/core/v8/V8Binding.h"
#include "core/dom/Document.h"
#include "core/frame/LocalFrame.h"
#include "modules/csspaint/PaintWorkletGlobalScope.h"

namespace blink {

// static
PaintWorklet* PaintWorklet::Create(LocalFrame* frame) {
  return new PaintWorklet(frame);
}

PaintWorklet::PaintWorklet(LocalFrame* frame)
    : Worklet(frame),
      paint_worklet_global_scope_(PaintWorkletGlobalScope::Create(
          frame,
          frame->GetDocument()->Url(),
          frame->GetDocument()->UserAgent(),
          frame->GetDocument()->GetSecurityOrigin(),
          ToIsolate(frame->GetDocument()))) {}

PaintWorklet::~PaintWorklet() {}

PaintWorkletGlobalScope* PaintWorklet::GetWorkletGlobalScopeProxy() const {
  return paint_worklet_global_scope_.Get();
}

CSSPaintDefinition* PaintWorklet::FindDefinition(const String& name) {
  return paint_worklet_global_scope_->FindDefinition(name);
}

void PaintWorklet::AddPendingGenerator(const String& name,
                                       CSSPaintImageGeneratorImpl* generator) {
  return paint_worklet_global_scope_->AddPendingGenerator(name, generator);
}

DEFINE_TRACE(PaintWorklet) {
  visitor->Trace(paint_worklet_global_scope_);
  Worklet::Trace(visitor);
}

}  // namespace blink
