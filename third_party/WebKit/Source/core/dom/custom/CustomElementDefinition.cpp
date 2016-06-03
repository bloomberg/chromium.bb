// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/custom/CustomElementDefinition.h"

namespace blink {

CustomElementDefinition::CustomElementDefinition(
    const CustomElementDescriptor& descriptor)
    : m_descriptor(descriptor)
{
}

CustomElementDefinition::~CustomElementDefinition()
{
}

DEFINE_TRACE(CustomElementDefinition)
{
    visitor->trace(m_constructionStack);
}

// https://html.spec.whatwg.org/multipage/scripting.html#concept-upgrade-an-element
void CustomElementDefinition::upgrade(Element* element)
{
    m_constructionStack.append(element);
    size_t depth = m_constructionStack.size();

    bool succeeded = runConstructor(element);

    // Pop the construction stack.
    if (m_constructionStack.last().get())
        DCHECK_EQ(m_constructionStack.last(), element);
    DCHECK_EQ(m_constructionStack.size(), depth); // It's a *stack*.
    m_constructionStack.removeLast();

    if (!succeeded)
        return;

    // TODO(dominicc): Turn this into an assertion when setting
    // 'custom' moves to the HTMLElement constructor. We will need to
    // add a bit for MARQUEE to be custom-gets-callbacks-yet-not-custom.
    element->setCustomElementState(CustomElementState::Custom);

    // TODO(dominicc): When the attributeChangedCallback is implemented,
    // enqueue reactions for attributes here.
    // TODO(dominicc): When the connectedCallback is implemented, enqueue
    // reactions here, if applicable.
}

bool CustomElementDefinition::runConstructor(Element*)
{
    return true;
}

} // namespace blink
