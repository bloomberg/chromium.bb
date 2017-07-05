// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/custom/CustomElementConnectedCallbackReaction.h"

#include "core/html/custom/CustomElementDefinition.h"

namespace blink {

CustomElementConnectedCallbackReaction::CustomElementConnectedCallbackReaction(
    CustomElementDefinition* definition)
    : CustomElementReaction(definition) {
  DCHECK(definition->HasConnectedCallback());
}

void CustomElementConnectedCallbackReaction::Invoke(Element* element) {
  definition_->RunConnectedCallback(element);
}

}  // namespace blink
