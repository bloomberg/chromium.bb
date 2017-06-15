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
    : Worklet(frame),
      pending_generator_registry_(new PaintWorkletPendingGeneratorRegistry) {}

PaintWorklet::~PaintWorklet() = default;

CSSPaintDefinition* PaintWorklet::FindDefinition(const String& name) {
  if (GetNumberOfGlobalScopes() == 0)
    return nullptr;

  PaintWorkletGlobalScopeProxy* proxy =
      PaintWorkletGlobalScopeProxy::From(FindAvailableGlobalScope());
  return proxy->FindDefinition(name);
}

void PaintWorklet::AddPendingGenerator(const String& name,
                                       CSSPaintImageGeneratorImpl* generator) {
  pending_generator_registry_->AddPendingGenerator(name, generator);
}

DEFINE_TRACE(PaintWorklet) {
  visitor->Trace(pending_generator_registry_);
  Worklet::Trace(visitor);
}

bool PaintWorklet::NeedsToCreateGlobalScope() {
  // "The user agent must have, and select from at least two
  // PaintWorkletGlobalScopes in the worklet's WorkletGlobalScopes list, unless
  // the user agent is under memory constraints."
  // https://drafts.css-houdini.org/css-paint-api-1/#drawing-an-image
  // TODO(nhiroki): In the current impl, we create only one global scope. We
  // should create at least two global scopes as the spec.
  return !GetNumberOfGlobalScopes();
}

WorkletGlobalScopeProxy* PaintWorklet::CreateGlobalScope() {
  return new PaintWorkletGlobalScopeProxy(
      ToDocument(GetExecutionContext())->GetFrame(),
      pending_generator_registry_);
}

}  // namespace blink
