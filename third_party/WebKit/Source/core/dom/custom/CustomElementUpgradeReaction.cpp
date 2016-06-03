// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/custom/CustomElementUpgradeReaction.h"

#include "core/dom/Element.h"
#include "core/dom/custom/CustomElementDefinition.h"

namespace blink {

CustomElementUpgradeReaction::CustomElementUpgradeReaction(
    CustomElementDefinition* definition)
    : m_definition(definition)
{
}

CustomElementUpgradeReaction::~CustomElementUpgradeReaction() = default;

DEFINE_TRACE(CustomElementUpgradeReaction)
{
    CustomElementReaction::trace(visitor);
    visitor->trace(m_definition);
}

void CustomElementUpgradeReaction::invoke(Element* element)
{
    m_definition->upgrade(element);
}

} // namespace blink
