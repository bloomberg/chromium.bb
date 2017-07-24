// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/csspaint/PaintWorklet.h"

#include "bindings/core/v8/V8BindingForCore.h"
#include "core/dom/Document.h"
#include "core/frame/LocalFrame.h"
#include "modules/csspaint/CSSPaintDefinition.h"
#include "modules/csspaint/PaintWorkletGlobalScope.h"
#include "platform/graphics/Image.h"

namespace blink {

static const DocumentPaintDefinition* kInvalidDocumentDefinition = nullptr;

// static
PaintWorklet* PaintWorklet::Create(LocalFrame* frame) {
  return new PaintWorklet(frame);
}

PaintWorklet::PaintWorklet(LocalFrame* frame)
    : Worklet(frame),
      pending_generator_registry_(new PaintWorkletPendingGeneratorRegistry) {}

PaintWorklet::~PaintWorklet() = default;

void PaintWorklet::AddPendingGenerator(const String& name,
                                       CSSPaintImageGeneratorImpl* generator) {
  pending_generator_registry_->AddPendingGenerator(name, generator);
}

RefPtr<Image> PaintWorklet::Paint(const String& name,
                                  const ImageResourceObserver& observer,
                                  const IntSize& size,
                                  const CSSStyleValueVector* data) {
  // TODO(xidachen): add policy for which global scope to select.
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

DEFINE_TRACE(PaintWorklet) {
  visitor->Trace(pending_generator_registry_);
  visitor->Trace(document_definition_map_);
  Worklet::Trace(visitor);
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
