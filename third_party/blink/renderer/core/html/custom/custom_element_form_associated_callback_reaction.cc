// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/custom/custom_element_form_associated_callback_reaction.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_definition.h"
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"

namespace blink {

CustomElementFormAssociatedCallbackReaction::
    CustomElementFormAssociatedCallbackReaction(
        CustomElementDefinition* definition,
        HTMLFormElement* nullable_form)
    : CustomElementReaction(definition), form_(nullable_form) {
  DCHECK(definition->HasFormAssociatedCallback());
}

void CustomElementFormAssociatedCallbackReaction::Trace(Visitor* visitor) {
  visitor->Trace(form_);
  CustomElementReaction::Trace(visitor);
}

void CustomElementFormAssociatedCallbackReaction::Invoke(Element* element) {
  definition_->RunFormAssociatedCallback(element, form_.Get());
}

}  // namespace blink
