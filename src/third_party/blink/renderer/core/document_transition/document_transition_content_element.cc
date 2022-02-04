// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/document_transition/document_transition_content_element.h"

#include "third_party/blink/renderer/core/layout/layout_document_transition_content.h"

namespace blink {

DocumentTransitionContentElement::DocumentTransitionContentElement(
    Element* parent,
    PseudoId pseudo_id,
    const AtomicString& document_transition_tag,
    viz::SharedElementResourceId resource_id)
    : DocumentTransitionPseudoElementBase(parent,
                                          pseudo_id,
                                          document_transition_tag),
      resource_id_(resource_id) {
  DCHECK(resource_id_.IsValid());
}

DocumentTransitionContentElement::~DocumentTransitionContentElement() = default;

void DocumentTransitionContentElement::SetIntrinsicSize(
    const LayoutSize& intrinsic_size) {
  intrinsic_size_ = intrinsic_size;
  if (auto* layout_object = GetLayoutObject()) {
    static_cast<LayoutDocumentTransitionContent*>(layout_object)
        ->OnIntrinsicSizeUpdated(intrinsic_size_);
  }
}

LayoutObject* DocumentTransitionContentElement::CreateLayoutObject(
    const ComputedStyle&,
    LegacyLayout) {
  return MakeGarbageCollected<LayoutDocumentTransitionContent>(this);
}

}  // namespace blink
