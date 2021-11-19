// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOCUMENT_TRANSITION_DOCUMENT_TRANSITION_CONTAINER_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOCUMENT_TRANSITION_DOCUMENT_TRANSITION_CONTAINER_ELEMENT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/document_transition/document_transition_content_element.h"
#include "third_party/blink/renderer/core/html/html_element.h"

namespace blink {

// This class implements the functionality to animate from a static snapshot of
// an element in previous DOM to a live snapshot of a corresponding element in
// the new DOM. These snapshots are represented using
// DocumentTransitionContainerElements.
//
// TODO(khushalsagar) : Switch this to a pseudo element.
class CORE_EXPORT DocumentTransitionContainerElement : public HTMLElement {
 public:
  explicit DocumentTransitionContainerElement(Document& document);
  ~DocumentTransitionContainerElement() override;

  void Prepare(Element* target_element);
  void PrepareResolved();
  void Start(Element* target_element);
  void StartFinished();

  const DocumentTransitionContentElement* old_content() const {
    return old_content_;
  }
  const DocumentTransitionContentElement* new_content() const {
    return new_content_.Get();
  }

  void Trace(Visitor* visitor) const override;

 private:
  class ResizeObserverDelegate;

  // Invoked when the element with the live snapshot is resized.
  void WasResized(const LayoutSize& new_size);

  Member<DocumentTransitionContentElement> old_content_;
  Member<DocumentTransitionContentElement> new_content_;

  // This is used to subscribe to changes in the size of the element's live
  // content.
  Member<ResizeObserver> resize_observer_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOCUMENT_TRANSITION_DOCUMENT_TRANSITION_CONTAINER_ELEMENT_H_
