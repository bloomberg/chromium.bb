// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/custom/CustomElement.h"

#include "core/HTMLElementFactory.h"
#include "core/HTMLElementTypeHelpers.h"
#include "core/dom/Document.h"
#include "core/dom/QualifiedName.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/html/HTMLElement.h"
#include "core/html/HTMLUnknownElement.h"
#include "core/html/custom/CEReactionsScope.h"
#include "core/html/custom/CustomElementDefinition.h"
#include "core/html/custom/CustomElementReactionStack.h"
#include "core/html/custom/CustomElementRegistry.h"
#include "core/html/custom/V0CustomElement.h"
#include "core/html/custom/V0CustomElementRegistrationContext.h"
#include "platform/wtf/text/AtomicStringHash.h"

namespace blink {

CustomElementRegistry* CustomElement::Registry(const Element& element) {
  return Registry(element.GetDocument());
}

CustomElementRegistry* CustomElement::Registry(const Document& document) {
  if (LocalDOMWindow* window = document.ExecutingWindow())
    return window->customElements();
  return nullptr;
}

static CustomElementDefinition* DefinitionForElementWithoutCheck(
    const Element& element) {
  DCHECK_EQ(element.GetCustomElementState(), CustomElementState::kCustom);
  return element.GetCustomElementDefinition();
}

CustomElementDefinition* CustomElement::DefinitionForElement(
    const Element* element) {
  if (!element ||
      element->GetCustomElementState() != CustomElementState::kCustom)
    return nullptr;
  return DefinitionForElementWithoutCheck(*element);
}

bool CustomElement::IsHyphenatedSpecElementName(const AtomicString& name) {
  // Even if Blink does not implement one of the related specs, (for
  // example annotation-xml is from MathML, which Blink does not
  // implement) we must prohibit using the name because that is
  // required by the HTML spec which we *do* implement. Don't remove
  // names from this list without removing them from the HTML spec
  // first.
  DEFINE_STATIC_LOCAL(HashSet<AtomicString>, hyphenated_spec_element_names,
                      ({
                          "annotation-xml", "color-profile", "font-face",
                          "font-face-src", "font-face-uri", "font-face-format",
                          "font-face-name", "missing-glyph",
                      }));
  return hyphenated_spec_element_names.Contains(name);
}

bool CustomElement::ShouldCreateCustomElement(const AtomicString& name) {
  return IsValidName(name);
}

bool CustomElement::ShouldCreateCustomElement(const QualifiedName& tag_name) {
  return ShouldCreateCustomElement(tag_name.LocalName()) &&
         tag_name.NamespaceURI() == HTMLNames::xhtmlNamespaceURI;
}

bool CustomElement::ShouldCreateCustomizedBuiltinElement(
    const AtomicString& local_name) {
  return htmlElementTypeForTag(local_name) !=
             HTMLElementType::kHTMLUnknownElement &&
         RuntimeEnabledFeatures::CustomElementsBuiltinEnabled();
}

bool CustomElement::ShouldCreateCustomizedBuiltinElement(
    const QualifiedName& tag_name) {
  return ShouldCreateCustomizedBuiltinElement(tag_name.LocalName()) &&
         tag_name.NamespaceURI() == HTMLNames::xhtmlNamespaceURI;
}

static CustomElementDefinition* DefinitionFor(
    const Document& document,
    const CustomElementDescriptor desc) {
  if (CustomElementRegistry* registry = CustomElement::Registry(document))
    return registry->DefinitionFor(desc);
  return nullptr;
}

HTMLElement* CustomElement::CreateCustomElementSync(
    Document& document,
    const QualifiedName& tag_name) {
  CustomElementDefinition* definition = nullptr;
  CustomElementRegistry* registry = CustomElement::Registry(document);
  if (registry) {
    definition = registry->DefinitionFor(
        CustomElementDescriptor(tag_name.LocalName(), tag_name.LocalName()));
  }
  return CreateCustomElementSync(document, tag_name, definition);
}

HTMLElement* CustomElement::CreateCustomElementSync(
    Document& document,
    const AtomicString& local_name,
    CustomElementDefinition* definition) {
  return CreateCustomElementSync(
      document,
      QualifiedName(g_null_atom, local_name, HTMLNames::xhtmlNamespaceURI),
      definition);
}

// https://dom.spec.whatwg.org/#concept-create-element
HTMLElement* CustomElement::CreateCustomElementSync(
    Document& document,
    const QualifiedName& tag_name,
    CustomElementDefinition* definition) {
  DCHECK(ShouldCreateCustomElement(tag_name) ||
         ShouldCreateCustomizedBuiltinElement(tag_name));
  HTMLElement* element;

  if (definition && definition->Descriptor().IsAutonomous()) {
    // 6. If definition is non-null and we have an autonomous custom element
    element = definition->CreateElementSync(document, tag_name);
  } else if (definition) {
    // 5. If definition is non-null and we have a customized built-in element
    element = CreateUndefinedElement(document, tag_name);
    definition->Upgrade(element);
  } else {
    // 7. Otherwise
    element = CreateUndefinedElement(document, tag_name);
  }
  return element;
}

HTMLElement* CustomElement::CreateCustomElementAsync(
    Document& document,
    const QualifiedName& tag_name) {
  DCHECK(ShouldCreateCustomElement(tag_name));

  // To create an element:
  // https://dom.spec.whatwg.org/#concept-create-element
  // 6. If definition is non-null, then:
  // 6.2. If the synchronous custom elements flag is not set:
  if (CustomElementDefinition* definition = DefinitionFor(
          document,
          CustomElementDescriptor(tag_name.LocalName(), tag_name.LocalName())))
    return definition->CreateElementAsync(document, tag_name);

  return CreateUndefinedElement(document, tag_name);
}

// Create a HTMLElement
HTMLElement* CustomElement::CreateUndefinedElement(
    Document& document,
    const QualifiedName& tag_name) {
  bool should_create_builtin = ShouldCreateCustomizedBuiltinElement(tag_name);
  DCHECK(ShouldCreateCustomElement(tag_name) || should_create_builtin);

  HTMLElement* element;
  if (V0CustomElement::IsValidName(tag_name.LocalName()) &&
      document.RegistrationContext()) {
    Element* v0element = document.RegistrationContext()->CreateCustomTagElement(
        document, tag_name);
    SECURITY_DCHECK(v0element->IsHTMLElement());
    element = ToHTMLElement(v0element);
  } else if (should_create_builtin) {
    element = HTMLElementFactory::createHTMLElement(
        tag_name.LocalName(), document, kCreatedByCreateElement);
  } else {
    element = HTMLElement::Create(tag_name, document);
  }

  element->SetCustomElementState(CustomElementState::kUndefined);

  return element;
}

HTMLElement* CustomElement::CreateFailedElement(Document& document,
                                                const QualifiedName& tag_name) {
  DCHECK(ShouldCreateCustomElement(tag_name));

  // "create an element for a token":
  // https://html.spec.whatwg.org/multipage/syntax.html#create-an-element-for-the-token

  // 7. If this step throws an exception, let element be instead a new element
  // that implements HTMLUnknownElement, with no attributes, namespace set to
  // given namespace, namespace prefix set to null, custom element state set
  // to "failed", and node document set to document.

  HTMLElement* element = HTMLUnknownElement::Create(tag_name, document);
  element->SetCustomElementState(CustomElementState::kFailed);
  return element;
}

void CustomElement::Enqueue(Element* element, CustomElementReaction* reaction) {
  // To enqueue an element on the appropriate element queue
  // https://html.spec.whatwg.org/multipage/scripting.html#enqueue-an-element-on-the-appropriate-element-queue

  // If the custom element reactions stack is not empty, then
  // Add element to the current element queue.
  if (CEReactionsScope* current = CEReactionsScope::Current()) {
    current->EnqueueToCurrentQueue(element, reaction);
    return;
  }

  // If the custom element reactions stack is empty, then
  // Add element to the backup element queue.
  CustomElementReactionStack::Current().EnqueueToBackupQueue(element, reaction);
}

void CustomElement::EnqueueConnectedCallback(Element* element) {
  CustomElementDefinition* definition =
      DefinitionForElementWithoutCheck(*element);
  if (definition->HasConnectedCallback())
    definition->EnqueueConnectedCallback(element);
}

void CustomElement::EnqueueDisconnectedCallback(Element* element) {
  CustomElementDefinition* definition =
      DefinitionForElementWithoutCheck(*element);
  if (definition->HasDisconnectedCallback())
    definition->EnqueueDisconnectedCallback(element);
}

void CustomElement::EnqueueAdoptedCallback(Element* element,
                                           Document* old_owner,
                                           Document* new_owner) {
  DCHECK_EQ(element->GetCustomElementState(), CustomElementState::kCustom);
  CustomElementDefinition* definition =
      DefinitionForElementWithoutCheck(*element);
  if (definition->HasAdoptedCallback())
    definition->EnqueueAdoptedCallback(element, old_owner, new_owner);
}

void CustomElement::EnqueueAttributeChangedCallback(
    Element* element,
    const QualifiedName& name,
    const AtomicString& old_value,
    const AtomicString& new_value) {
  CustomElementDefinition* definition =
      DefinitionForElementWithoutCheck(*element);
  if (definition->HasAttributeChangedCallback(name))
    definition->EnqueueAttributeChangedCallback(element, name, old_value,
                                                new_value);
}

void CustomElement::TryToUpgrade(Element* element) {
  // Try to upgrade an element
  // https://html.spec.whatwg.org/multipage/scripting.html#concept-try-upgrade

  DCHECK_EQ(element->GetCustomElementState(), CustomElementState::kUndefined);

  CustomElementRegistry* registry = CustomElement::Registry(*element);
  if (!registry)
    return;
  if (CustomElementDefinition* definition = registry->DefinitionFor(
          CustomElementDescriptor(element->localName(), element->localName())))
    definition->EnqueueUpgradeReaction(element);
  else
    registry->AddCandidate(element);
}

}  // namespace blink
