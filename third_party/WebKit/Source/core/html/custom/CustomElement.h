// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CustomElement_h
#define CustomElement_h

#include "core/CoreExport.h"
#include "core/dom/Element.h"
#include "platform/text/Character.h"
#include "platform/wtf/ASCIICType.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/text/AtomicString.h"

namespace blink {

class Document;
class Element;
class HTMLElement;
class QualifiedName;
class CustomElementDefinition;
class CustomElementReaction;
class CustomElementRegistry;

class CORE_EXPORT CustomElement {
  STATIC_ONLY(CustomElement);

 public:
  // Retrieves the CustomElementRegistry for Element, if any. This
  // may be a different object for a given element over its lifetime
  // as it moves between documents.
  static CustomElementRegistry* Registry(const Element&);
  static CustomElementRegistry* Registry(const Document&);

  static CustomElementDefinition* DefinitionForElement(const Element*);

  static bool IsValidName(const AtomicString& name) {
    // This quickly rejects all common built-in element names.
    if (name.find('-', 1) == kNotFound)
      return false;

    if (!IsASCIILower(name[0]))
      return false;

    if (name.Is8Bit()) {
      const LChar* characters = name.Characters8();
      for (size_t i = 1; i < name.length(); ++i) {
        if (!Character::IsPotentialCustomElementName8BitChar(characters[i]))
          return false;
      }
    } else {
      const UChar* characters = name.Characters16();
      for (size_t i = 1; i < name.length();) {
        UChar32 ch;
        U16_NEXT(characters, i, name.length(), ch);
        if (!Character::IsPotentialCustomElementNameChar(ch))
          return false;
      }
    }

    return !IsHyphenatedSpecElementName(name);
  }

  static bool ShouldCreateCustomElement(const AtomicString& local_name);
  static bool ShouldCreateCustomElement(const QualifiedName&);
  static bool ShouldCreateCustomizedBuiltinElement(
      const AtomicString& local_name);
  static bool ShouldCreateCustomizedBuiltinElement(const QualifiedName&);

  static HTMLElement* CreateCustomElementSync(Document&, const QualifiedName&);
  static HTMLElement* CreateCustomElementSync(Document&,
                                              const AtomicString& local_name,
                                              CustomElementDefinition*);
  static HTMLElement* CreateCustomElementSync(Document&,
                                              const QualifiedName&,
                                              CustomElementDefinition*);
  static HTMLElement* CreateCustomElementAsync(Document&, const QualifiedName&);

  static HTMLElement* CreateFailedElement(Document&, const QualifiedName&);

  static void Enqueue(Element*, CustomElementReaction*);
  static void EnqueueConnectedCallback(Element*);
  static void EnqueueDisconnectedCallback(Element*);
  static void EnqueueAdoptedCallback(Element*,
                                     Document* old_owner,
                                     Document* new_owner);
  static void EnqueueAttributeChangedCallback(Element*,
                                              const QualifiedName&,
                                              const AtomicString& old_value,
                                              const AtomicString& new_value);

  static void TryToUpgrade(Element*);

 private:
  // Some existing specs have element names with hyphens in them,
  // like font-face in SVG. The custom elements spec explicitly
  // disallows these as custom element names.
  // https://html.spec.whatwg.org/#valid-custom-element-name
  static bool IsHyphenatedSpecElementName(const AtomicString&);
  static HTMLElement* CreateUndefinedElement(Document&, const QualifiedName&);
};

}  // namespace blink

#endif  // CustomElement_h
