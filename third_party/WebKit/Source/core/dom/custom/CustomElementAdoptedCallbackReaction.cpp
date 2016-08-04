// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/custom/CustomElementAdoptedCallbackReaction.h"

#include "core/dom/custom/CustomElementDefinition.h"

namespace blink {

CustomElementAdoptedCallbackReaction::CustomElementAdoptedCallbackReaction(
    CustomElementDefinition* definition)
    : CustomElementReaction(definition)
{
    DCHECK(definition->hasAdoptedCallback());
}

void CustomElementAdoptedCallbackReaction::invoke(Element* element)
{
    m_definition->runAdoptedCallback(element);
}

} // namespace blink
