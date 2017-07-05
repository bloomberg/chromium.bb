// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/custom/CustomElementDisconnectedCallbackReaction.h"

#include "core/html/custom/CustomElementDefinition.h"

namespace blink {

CustomElementDisconnectedCallbackReaction::
    CustomElementDisconnectedCallbackReaction(
        CustomElementDefinition* definition)
    : CustomElementReaction(definition) {
  DCHECK(definition->HasDisconnectedCallback());
}

void CustomElementDisconnectedCallbackReaction::Invoke(Element* element) {
  definition_->RunDisconnectedCallback(element);
}

}  // namespace blink
