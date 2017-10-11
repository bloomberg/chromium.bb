/*
 * Copyright (c) 2012 Motorola Mobility, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY MOTOROLA MOBILITY, INC. AND ITS CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MOTOROLA MOBILITY, INC. OR ITS
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/html/forms/RadioNodeList.h"

#include "core/dom/Element.h"
#include "core/dom/NodeRareData.h"
#include "core/html/HTMLImageElement.h"
#include "core/html/HTMLObjectElement.h"
#include "core/html/forms/HTMLFormElement.h"
#include "core/html/forms/HTMLInputElement.h"
#include "core/html_names.h"
#include "core/input_type_names.h"

namespace blink {

using namespace HTMLNames;

RadioNodeList::RadioNodeList(ContainerNode& root_node,
                             const AtomicString& name,
                             CollectionType type)
    : LiveNodeList(root_node,
                   type,
                   kInvalidateForFormControls,
                   IsHTMLFormElement(root_node) ? NodeListRootType::kTreeScope
                                                : NodeListRootType::kNode),
      name_(name) {}

RadioNodeList::~RadioNodeList() {}

static inline HTMLInputElement* ToRadioButtonInputElement(Element& element) {
  if (!IsHTMLInputElement(element))
    return nullptr;
  HTMLInputElement& input_element = ToHTMLInputElement(element);
  if (input_element.type() != InputTypeNames::radio ||
      input_element.value().IsEmpty())
    return nullptr;
  return &input_element;
}

String RadioNodeList::value() const {
  if (ShouldOnlyMatchImgElements())
    return String();
  unsigned length = this->length();
  for (unsigned i = 0; i < length; ++i) {
    const HTMLInputElement* input_element = ToRadioButtonInputElement(*item(i));
    if (!input_element || !input_element->checked())
      continue;
    return input_element->value();
  }
  return String();
}

void RadioNodeList::setValue(const String& value) {
  if (ShouldOnlyMatchImgElements())
    return;
  unsigned length = this->length();
  for (unsigned i = 0; i < length; ++i) {
    HTMLInputElement* input_element = ToRadioButtonInputElement(*item(i));
    if (!input_element || input_element->value() != value)
      continue;
    input_element->setChecked(true);
    return;
  }
}

bool RadioNodeList::MatchesByIdOrName(const Element& test_element) const {
  return test_element.GetIdAttribute() == name_ ||
         test_element.GetNameAttribute() == name_;
}

bool RadioNodeList::CheckElementMatchesRadioNodeListFilter(
    const Element& test_element) const {
  DCHECK(!ShouldOnlyMatchImgElements());
  DCHECK(IsHTMLObjectElement(test_element) ||
         test_element.IsFormControlElement());
  if (IsHTMLFormElement(ownerNode())) {
    HTMLFormElement* form_element = ToHTMLElement(test_element).formOwner();
    if (!form_element || form_element != ownerNode())
      return false;
  }

  return MatchesByIdOrName(test_element);
}

bool RadioNodeList::ElementMatches(const Element& element) const {
  if (ShouldOnlyMatchImgElements()) {
    if (!IsHTMLImageElement(element))
      return false;

    if (ToHTMLImageElement(element).formOwner() != ownerNode())
      return false;

    return MatchesByIdOrName(element);
  }

  if (!IsHTMLObjectElement(element) && !element.IsFormControlElement())
    return false;

  if (IsHTMLInputElement(element) &&
      ToHTMLInputElement(element).type() == InputTypeNames::image)
    return false;

  return CheckElementMatchesRadioNodeListFilter(element);
}

}  // namespace blink
