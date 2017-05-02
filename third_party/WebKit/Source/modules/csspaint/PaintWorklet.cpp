// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/csspaint/PaintWorklet.h"

#include "bindings/core/v8/V8BindingForCore.h"
#include "core/dom/Document.h"
#include "core/frame/LocalFrame.h"
#include "modules/csspaint/PaintWorkletGlobalScope.h"

namespace blink {

// static
PaintWorklet* PaintWorklet::Create(LocalFrame* frame) {
  return new PaintWorklet(frame);
}

PaintWorklet::PaintWorklet(LocalFrame* frame)
    : MainThreadWorklet(frame),
      global_scope_proxy_(
          WTF::MakeUnique<PaintWorkletGlobalScopeProxy>(frame)) {}

PaintWorklet::~PaintWorklet() = default;

WorkletGlobalScopeProxy* PaintWorklet::GetWorkletGlobalScopeProxy() const {
  return global_scope_proxy_.get();
}

CSSPaintDefinition* PaintWorklet::FindDefinition(const String& name) {
  return global_scope_proxy_->FindDefinition(name);
}

void PaintWorklet::AddPendingGenerator(const String& name,
                                       CSSPaintImageGeneratorImpl* generator) {
  return global_scope_proxy_->AddPendingGenerator(name, generator);
}

DEFINE_TRACE(PaintWorklet) {
  MainThreadWorklet::Trace(visitor);
}

}  // namespace blink
