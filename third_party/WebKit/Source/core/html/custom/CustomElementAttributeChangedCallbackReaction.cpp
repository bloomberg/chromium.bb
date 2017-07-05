// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/custom/CustomElementAttributeChangedCallbackReaction.h"

#include "core/html/custom/CustomElementDefinition.h"

namespace blink {

CustomElementAttributeChangedCallbackReaction::
    CustomElementAttributeChangedCallbackReaction(
        CustomElementDefinition* definition,
        const QualifiedName& name,
        const AtomicString& old_value,
        const AtomicString& new_value)
    : CustomElementReaction(definition),
      name_(name),
      old_value_(old_value),
      new_value_(new_value) {
  DCHECK(definition->HasAttributeChangedCallback(name));
}

void CustomElementAttributeChangedCallbackReaction::Invoke(Element* element) {
  definition_->RunAttributeChangedCallback(element, name_, old_value_,
                                           new_value_);
}

}  // namespace blink
