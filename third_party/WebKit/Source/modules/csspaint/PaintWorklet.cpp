// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/csspaint/PaintWorklet.h"

#include "bindings/core/v8/V8BindingForCore.h"
#include "core/dom/Document.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "modules/csspaint/CSSPaintDefinition.h"
#include "modules/csspaint/PaintWorkletGlobalScope.h"
#include "platform/graphics/Image.h"

namespace blink {

const size_t PaintWorklet::kNumGlobalScopes = 2u;
DocumentPaintDefinition* const kInvalidDocumentDefinition = nullptr;
// We use each global scope for many frames to try to maximize cache hits.
const size_t kFrameCountToSwitch = 120u;

// static
PaintWorklet* PaintWorklet::From(LocalDOMWindow& window) {
  PaintWorklet* supplement = static_cast<PaintWorklet*>(
      Supplement<LocalDOMWindow>::From(window, SupplementName()));
  if (!supplement && window.GetFrame()) {
    supplement = Create(window.GetFrame());
    ProvideTo(window, SupplementName(), supplement);
  }
  return supplement;
}

// static
PaintWorklet* PaintWorklet::Create(LocalFrame* frame) {
  return new PaintWorklet(frame);
}

PaintWorklet::PaintWorklet(LocalFrame* frame)
    : Worklet(frame),
      Supplement<LocalDOMWindow>(*frame->DomWindow()),
      pending_generator_registry_(new PaintWorkletPendingGeneratorRegistry) {}

PaintWorklet::~PaintWorklet() = default;

void PaintWorklet::AddPendingGenerator(const String& name,
                                       CSSPaintImageGeneratorImpl* generator) {
  pending_generator_registry_->AddPendingGenerator(name, generator);
}

// For this document, we try to check how many times there is a repaint, which
// represents how many frames have executed this paint function. Then for every
// 120 frames, we switch to another global scope, to enforce the constraint that
// we can't rely on the state of the paint worklet global scope.
size_t PaintWorklet::SelectGlobalScope() const {
  size_t current_paint_frame_count = GetFrame()->View()->PaintFrameCount();
  return (current_paint_frame_count / kFrameCountToSwitch) % kNumGlobalScopes;
}

RefPtr<Image> PaintWorklet::Paint(const String& name,
                                  const ImageResourceObserver& observer,
                                  const IntSize& size,
                                  const CSSStyleValueVector* data) {
  if (!document_definition_map_.Contains(name))
    return nullptr;

  // Check if the existing document definition is valid or not.
  DocumentPaintDefinition* document_definition =
      document_definition_map_.at(name);
  if (document_definition == kInvalidDocumentDefinition)
    return nullptr;

  PaintWorkletGlobalScopeProxy* proxy =
      PaintWorkletGlobalScopeProxy::From(FindAvailableGlobalScope());
  CSSPaintDefinition* paint_definition = proxy->FindDefinition(name);
  return paint_definition->Paint(observer, size, data);
}

const char* PaintWorklet::SupplementName() {
  return "PaintWorklet";
}

DEFINE_TRACE(PaintWorklet) {
  visitor->Trace(pending_generator_registry_);
  visitor->Trace(document_definition_map_);
  Worklet::Trace(visitor);
  Supplement<LocalDOMWindow>::Trace(visitor);
}

bool PaintWorklet::NeedsToCreateGlobalScope() {
  return GetNumberOfGlobalScopes() < kNumGlobalScopes;
}

WorkletGlobalScopeProxy* PaintWorklet::CreateGlobalScope() {
  DCHECK(NeedsToCreateGlobalScope());
  return new PaintWorkletGlobalScopeProxy(
      ToDocument(GetExecutionContext())->GetFrame(),
      pending_generator_registry_, GetNumberOfGlobalScopes() + 1);
}

}  // namespace blink
