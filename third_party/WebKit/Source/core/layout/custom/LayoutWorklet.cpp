// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/custom/LayoutWorklet.h"

#include "bindings/core/v8/V8BindingForCore.h"
#include "core/dom/Document.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/layout/custom/DocumentLayoutDefinition.h"
#include "core/layout/custom/LayoutWorkletGlobalScopeProxy.h"
#include "core/layout/custom/PendingLayoutRegistry.h"

namespace blink {

const size_t LayoutWorklet::kNumGlobalScopes = 2u;
DocumentLayoutDefinition* const kInvalidDocumentLayoutDefinition = nullptr;

// static
LayoutWorklet* LayoutWorklet::From(LocalDOMWindow& window) {
  LayoutWorklet* supplement =
      Supplement<LocalDOMWindow>::From<LayoutWorklet>(window);
  if (!supplement && window.GetFrame()) {
    supplement = Create(window.GetFrame());
    ProvideTo(window, supplement);
  }
  return supplement;
}

// static
LayoutWorklet* LayoutWorklet::Create(LocalFrame* frame) {
  return new LayoutWorklet(frame);
}

LayoutWorklet::LayoutWorklet(LocalFrame* frame)
    : Worklet(frame->GetDocument()),
      Supplement<LocalDOMWindow>(*frame->DomWindow()),
      pending_layout_registry_(new PendingLayoutRegistry()) {}

LayoutWorklet::~LayoutWorklet() = default;

const char LayoutWorklet::kSupplementName[] = "LayoutWorklet";

void LayoutWorklet::AddPendingLayout(const AtomicString& name, Node* node) {
  pending_layout_registry_->AddPendingLayout(name, node);
}

LayoutWorkletGlobalScopeProxy* LayoutWorklet::Proxy() {
  return LayoutWorkletGlobalScopeProxy::From(FindAvailableGlobalScope());
}

void LayoutWorklet::Trace(blink::Visitor* visitor) {
  visitor->Trace(document_definition_map_);
  visitor->Trace(pending_layout_registry_);
  Worklet::Trace(visitor);
  Supplement<LocalDOMWindow>::Trace(visitor);
}

bool LayoutWorklet::NeedsToCreateGlobalScope() {
  return GetNumberOfGlobalScopes() < kNumGlobalScopes;
}

WorkletGlobalScopeProxy* LayoutWorklet::CreateGlobalScope() {
  DCHECK(NeedsToCreateGlobalScope());
  return new LayoutWorkletGlobalScopeProxy(
      ToDocument(GetExecutionContext())->GetFrame(), ModuleResponsesMap(),
      pending_layout_registry_, GetNumberOfGlobalScopes() + 1);
}

}  // namespace blink
