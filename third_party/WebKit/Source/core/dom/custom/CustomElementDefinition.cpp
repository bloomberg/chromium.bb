// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/custom/CustomElementDefinition.h"

#include "core/dom/custom/CEReactionsScope.h"
#include "core/dom/custom/CustomElement.h"
#include "core/dom/custom/CustomElementAttributeChangedCallbackReaction.h"
#include "core/dom/custom/CustomElementConnectedCallbackReaction.h"
#include "core/dom/custom/CustomElementDisconnectedCallbackReaction.h"
#include "core/dom/custom/CustomElementUpgradeReaction.h"

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
    // TODO(kojii): This should be reversed by exposing observedAttributes from
    // ScriptCustomElementDefinition, because Element::attributes() requires
    // attribute synchronizations, and generally elements have more attributes
    // than custom elements observe.
    for (const auto& attribute : element->attributes()) {
        if (hasAttributeChangedCallback(attribute.name()))
            enqueueAttributeChangedCallback(element, attribute.name(), nullAtom, attribute.value());
    }

    if (element->inShadowIncludingDocument() && hasConnectedCallback())
        enqueueConnectedCallback(element);

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

    CHECK(element->getCustomElementState() == CustomElementState::Custom);
}

static void enqueueReaction(Element* element, CustomElementReaction* reaction)
{
    // CEReactionsScope must be created by [CEReactions] in IDL,
    // or callers must setup explicitly if it does not go through bindings.
    DCHECK(CEReactionsScope::current());
    CEReactionsScope::current()->enqueue(element, reaction);
}

void CustomElementDefinition::enqueueUpgradeReaction(Element* element)
{
    enqueueReaction(element, new CustomElementUpgradeReaction(this));
}

void CustomElementDefinition::enqueueConnectedCallback(Element* element)
{
    enqueueReaction(element, new CustomElementConnectedCallbackReaction(this));
}

void CustomElementDefinition::enqueueDisconnectedCallback(Element* element)
{
    enqueueReaction(element, new CustomElementDisconnectedCallbackReaction(this));
}

void CustomElementDefinition::enqueueAttributeChangedCallback(Element* element,
    const QualifiedName& name,
    const AtomicString& oldValue, const AtomicString& newValue)
{
    enqueueReaction(element, new CustomElementAttributeChangedCallbackReaction(this, name, oldValue, newValue));
}

} // namespace blink
