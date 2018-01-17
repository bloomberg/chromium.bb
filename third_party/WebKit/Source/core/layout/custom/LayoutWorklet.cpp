// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/custom/LayoutWorklet.h"

#include "bindings/core/v8/V8BindingForCore.h"
#include "core/dom/Document.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/layout/custom/LayoutWorkletGlobalScopeProxy.h"

namespace blink {

const size_t LayoutWorklet::kNumGlobalScopes = 2u;

// static
LayoutWorklet* LayoutWorklet::From(LocalDOMWindow& window) {
  LayoutWorklet* supplement = static_cast<LayoutWorklet*>(
      Supplement<LocalDOMWindow>::From(window, SupplementName()));
  if (!supplement && window.GetFrame()) {
    supplement = Create(window.GetFrame());
    ProvideTo(window, SupplementName(), supplement);
  }
  return supplement;
}

// static
LayoutWorklet* LayoutWorklet::Create(LocalFrame* frame) {
  return new LayoutWorklet(frame);
}

LayoutWorklet::LayoutWorklet(LocalFrame* frame)
    : Worklet(frame->GetDocument()),
      Supplement<LocalDOMWindow>(*frame->DomWindow()) {}

LayoutWorklet::~LayoutWorklet() = default;

const char* LayoutWorklet::SupplementName() {
  return "LayoutWorklet";
}

void LayoutWorklet::Trace(blink::Visitor* visitor) {
  Worklet::Trace(visitor);
  Supplement<LocalDOMWindow>::Trace(visitor);
}

bool LayoutWorklet::NeedsToCreateGlobalScope() {
  return GetNumberOfGlobalScopes() < kNumGlobalScopes;
}

WorkletGlobalScopeProxy* LayoutWorklet::CreateGlobalScope() {
  DCHECK(NeedsToCreateGlobalScope());
  return new LayoutWorkletGlobalScopeProxy(
      ToDocument(GetExecutionContext())->GetFrame(),
      GetNumberOfGlobalScopes() + 1);
}

}  // namespace blink
