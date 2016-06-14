// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/custom/CustomElementDefinition.h"

#include "core/dom/ExceptionCode.h"
#include "core/dom/custom/CEReactionsScope.h"
#include "core/dom/custom/CustomElement.h"
#include "core/dom/custom/CustomElementAttributeChangedCallbackReaction.h"
#include "core/dom/custom/CustomElementConnectedCallbackReaction.h"
#include "core/dom/custom/CustomElementDisconnectedCallbackReaction.h"
#include "core/dom/custom/CustomElementUpgradeReaction.h"
#include "core/html/HTMLElement.h"

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

static String errorMessageForConstructorResult(Element* element,
    Document& document, const QualifiedName& tagName)
{
    // https://dom.spec.whatwg.org/#concept-create-element
    // 6.1.4. If result's attribute list is not empty, then throw a NotSupportedError.
    if (element->hasAttributes())
        return "The result must not have attributes";
    // 6.1.5. If result has children, then throw a NotSupportedError.
    if (element->hasChildren())
        return "The result must not have children";
    // 6.1.6. If result's parent is not null, then throw a NotSupportedError.
    if (element->parentNode())
        return "The result must not have a parent";
    // 6.1.7. If result's node document is not document, then throw a NotSupportedError.
    if (&element->document() != &document)
        return "The result must be in the same document";
    // 6.1.8. If result's namespace is not the HTML namespace, then throw a NotSupportedError.
    if (element->namespaceURI() != HTMLNames::xhtmlNamespaceURI)
        return "The result must have HTML namespace";
    // 6.1.9. If result's local name is not equal to localName, then throw a NotSupportedError.
    if (element->localName() != tagName.localName())
        return "The result must have the same localName";
    return String();
}

void CustomElementDefinition::checkConstructorResult(Element* element,
    Document& document, const QualifiedName& tagName,
    ExceptionState& exceptionState)
{
    // https://dom.spec.whatwg.org/#concept-create-element
    // 6.1.3. If result does not implement the HTMLElement interface, throw a TypeError.
    // See https://github.com/whatwg/html/issues/1402 for more clarifications.
    if (!element || !element->isHTMLElement()) {
        exceptionState.throwTypeError("The result must implement HTMLElement interface");
        return;
    }

    // 6.1.4. through 6.1.9.
    const String message = errorMessageForConstructorResult(element, document, tagName);
    if (!message.isEmpty())
        exceptionState.throwDOMException(NotSupportedError, message);
}

HTMLElement* CustomElementDefinition::createElementAsync(Document& document, const QualifiedName& tagName)
{
    // https://dom.spec.whatwg.org/#concept-create-element
    // 6. If definition is non-null, then:
    // 6.2. If the synchronous custom elements flag is not set:
    // 6.2.1. Set result to a new element that implements the HTMLElement
    // interface, with no attributes, namespace set to the HTML namespace,
    // namespace prefix set to prefix, local name set to localName, custom
    // element state set to "undefined", and node document set to document.
    HTMLElement* element = HTMLElement::create(tagName, document);
    element->setCustomElementState(CustomElementState::Undefined);
    // 6.2.2. Enqueue a custom element upgrade reaction given result and
    // definition.
    enqueueUpgradeReaction(element);
    return element;
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
