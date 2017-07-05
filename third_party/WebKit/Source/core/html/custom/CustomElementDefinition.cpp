// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/custom/CustomElementDefinition.h"

#include "core/dom/Attr.h"
#include "core/dom/ExceptionCode.h"
#include "core/html/HTMLElement.h"
#include "core/html/custom/CustomElement.h"
#include "core/html/custom/CustomElementAdoptedCallbackReaction.h"
#include "core/html/custom/CustomElementAttributeChangedCallbackReaction.h"
#include "core/html/custom/CustomElementConnectedCallbackReaction.h"
#include "core/html/custom/CustomElementDisconnectedCallbackReaction.h"
#include "core/html/custom/CustomElementReaction.h"
#include "core/html/custom/CustomElementReactionStack.h"
#include "core/html/custom/CustomElementUpgradeReaction.h"

namespace blink {

CustomElementDefinition::CustomElementDefinition(
    const CustomElementDescriptor& descriptor)
    : descriptor_(descriptor) {}

CustomElementDefinition::CustomElementDefinition(
    const CustomElementDescriptor& descriptor,
    const HashSet<AtomicString>& observed_attributes)
    : descriptor_(descriptor),
      observed_attributes_(observed_attributes),
      has_style_attribute_changed_callback_(
          observed_attributes.Contains(HTMLNames::styleAttr.LocalName())) {}

CustomElementDefinition::~CustomElementDefinition() {}

DEFINE_TRACE(CustomElementDefinition) {
  visitor->Trace(construction_stack_);
}

static String ErrorMessageForConstructorResult(Element* element,
                                               Document& document,
                                               const QualifiedName& tag_name) {
  // https://dom.spec.whatwg.org/#concept-create-element
  // 6.1.4. If result's attribute list is not empty, then throw a
  // NotSupportedError.
  if (element->hasAttributes())
    return "The result must not have attributes";
  // 6.1.5. If result has children, then throw a NotSupportedError.
  if (element->HasChildren())
    return "The result must not have children";
  // 6.1.6. If result's parent is not null, then throw a NotSupportedError.
  if (element->parentNode())
    return "The result must not have a parent";
  // 6.1.7. If result's node document is not document, then throw a
  // NotSupportedError.
  if (&element->GetDocument() != &document)
    return "The result must be in the same document";
  // 6.1.8. If result's namespace is not the HTML namespace, then throw a
  // NotSupportedError.
  if (element->namespaceURI() != HTMLNames::xhtmlNamespaceURI)
    return "The result must have HTML namespace";
  // 6.1.9. If result's local name is not equal to localName, then throw a
  // NotSupportedError.
  if (element->localName() != tag_name.LocalName())
    return "The result must have the same localName";
  return String();
}

void CustomElementDefinition::CheckConstructorResult(
    Element* element,
    Document& document,
    const QualifiedName& tag_name,
    ExceptionState& exception_state) {
  // https://dom.spec.whatwg.org/#concept-create-element
  // 6.1.3. If result does not implement the HTMLElement interface, throw a
  // TypeError.
  // See https://github.com/whatwg/html/issues/1402 for more clarifications.
  if (!element || !element->IsHTMLElement()) {
    exception_state.ThrowTypeError(
        "The result must implement HTMLElement interface");
    return;
  }

  // 6.1.4. through 6.1.9.
  const String message =
      ErrorMessageForConstructorResult(element, document, tag_name);
  if (!message.IsEmpty())
    exception_state.ThrowDOMException(kNotSupportedError, message);
}

HTMLElement* CustomElementDefinition::CreateElementForConstructor(
    Document& document) {
  // TODO(kojii): When HTMLElementFactory has an option not to queue
  // upgrade, call that instead of HTMLElement. HTMLElement is enough
  // for now, but type extension will require HTMLElementFactory.
  HTMLElement* element =
      HTMLElement::Create(QualifiedName(g_null_atom, Descriptor().LocalName(),
                                        HTMLNames::xhtmlNamespaceURI),
                          document);
  // TODO(davaajav): write this as one call to setCustomElementState instead of
  // two
  element->SetCustomElementState(CustomElementState::kUndefined);
  element->SetCustomElementDefinition(this);
  return element;
}

HTMLElement* CustomElementDefinition::CreateElementAsync(
    Document& document,
    const QualifiedName& tag_name) {
  // https://dom.spec.whatwg.org/#concept-create-element
  // 6. If definition is non-null, then:
  // 6.2. If the synchronous custom elements flag is not set:
  // 6.2.1. Set result to a new element that implements the HTMLElement
  // interface, with no attributes, namespace set to the HTML namespace,
  // namespace prefix set to prefix, local name set to localName, custom
  // element state set to "undefined", and node document set to document.
  HTMLElement* element = HTMLElement::Create(tag_name, document);
  element->SetCustomElementState(CustomElementState::kUndefined);
  // 6.2.2. Enqueue a custom element upgrade reaction given result and
  // definition.
  EnqueueUpgradeReaction(element);
  return element;
}

CustomElementDefinition::ConstructionStackScope::ConstructionStackScope(
    CustomElementDefinition* definition,
    Element* element)
    : construction_stack_(definition->construction_stack_), element_(element) {
  // Push the construction stack.
  construction_stack_.push_back(element);
  depth_ = construction_stack_.size();
}

CustomElementDefinition::ConstructionStackScope::~ConstructionStackScope() {
  // Pop the construction stack.
  DCHECK(!construction_stack_.back() || construction_stack_.back() == element_);
  DCHECK_EQ(construction_stack_.size(), depth_);  // It's a *stack*.
  construction_stack_.pop_back();
}

// https://html.spec.whatwg.org/multipage/scripting.html#concept-upgrade-an-element
void CustomElementDefinition::Upgrade(Element* element) {
  DCHECK_EQ(element->GetCustomElementState(), CustomElementState::kUndefined);

  if (!observed_attributes_.IsEmpty())
    EnqueueAttributeChangedCallbackForAllAttributes(element);

  if (element->isConnected() && HasConnectedCallback())
    EnqueueConnectedCallback(element);

  bool succeeded = false;
  {
    ConstructionStackScope construction_stack_scope(this, element);
    succeeded = RunConstructor(element);
  }
  if (!succeeded) {
    element->SetCustomElementState(CustomElementState::kFailed);
    CustomElementReactionStack::Current().ClearQueue(element);
    return;
  }

  element->SetCustomElementDefinition(this);
}

bool CustomElementDefinition::HasAttributeChangedCallback(
    const QualifiedName& name) const {
  return observed_attributes_.Contains(name.LocalName());
}

bool CustomElementDefinition::HasStyleAttributeChangedCallback() const {
  return has_style_attribute_changed_callback_;
}

void CustomElementDefinition::EnqueueUpgradeReaction(Element* element) {
  CustomElement::Enqueue(element, new CustomElementUpgradeReaction(this));
}

void CustomElementDefinition::EnqueueConnectedCallback(Element* element) {
  CustomElement::Enqueue(element,
                         new CustomElementConnectedCallbackReaction(this));
}

void CustomElementDefinition::EnqueueDisconnectedCallback(Element* element) {
  CustomElement::Enqueue(element,
                         new CustomElementDisconnectedCallbackReaction(this));
}

void CustomElementDefinition::EnqueueAdoptedCallback(Element* element,
                                                     Document* old_document,
                                                     Document* new_document) {
  CustomElementReaction* reaction = new CustomElementAdoptedCallbackReaction(
      this, old_document, new_document);
  CustomElement::Enqueue(element, reaction);
}

void CustomElementDefinition::EnqueueAttributeChangedCallback(
    Element* element,
    const QualifiedName& name,
    const AtomicString& old_value,
    const AtomicString& new_value) {
  CustomElement::Enqueue(element,
                         new CustomElementAttributeChangedCallbackReaction(
                             this, name, old_value, new_value));
}

void CustomElementDefinition::EnqueueAttributeChangedCallbackForAllAttributes(
    Element* element) {
  // Avoid synchronizing all attributes unless it is needed, while enqueing
  // callbacks "in order" as defined in the spec.
  // https://html.spec.whatwg.org/multipage/scripting.html#concept-upgrade-an-element
  for (const AtomicString& name : observed_attributes_)
    element->SynchronizeAttribute(name);
  for (const auto& attribute : element->AttributesWithoutUpdate()) {
    if (HasAttributeChangedCallback(attribute.GetName())) {
      EnqueueAttributeChangedCallback(element, attribute.GetName(), g_null_atom,
                                      attribute.Value());
    }
  }
}

}  // namespace blink
