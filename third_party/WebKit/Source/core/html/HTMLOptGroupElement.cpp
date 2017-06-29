/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2010 Apple Inc. All rights
 * reserved.
 *           (C) 2006 Alexey Proskuryakov (ap@nypop.com)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "core/html/HTMLOptGroupElement.h"

#include "core/HTMLNames.h"
#include "core/dom/Text.h"
#include "core/editing/EditingUtilities.h"
#include "core/html/HTMLContentElement.h"
#include "core/html/HTMLDivElement.h"
#include "core/html/HTMLSelectElement.h"
#include "core/html/shadow/ShadowElementNames.h"
#include "platform/wtf/StdLibExtras.h"
#include "platform/wtf/text/CharacterNames.h"

namespace blink {

using namespace HTMLNames;

inline HTMLOptGroupElement::HTMLOptGroupElement(Document& document)
    : HTMLElement(optgroupTag, document) {
}

// An explicit empty destructor should be in HTMLOptGroupElement.cpp, because
// if an implicit destructor is used or an empty destructor is defined in
// HTMLOptGroupElement.h, when including HTMLOptGroupElement.h,
// msvc tries to expand the destructor and causes
// a compile error because of lack of ComputedStyle definition.
HTMLOptGroupElement::~HTMLOptGroupElement() {}

HTMLOptGroupElement* HTMLOptGroupElement::Create(Document& document) {
  HTMLOptGroupElement* opt_group_element = new HTMLOptGroupElement(document);
  opt_group_element->EnsureUserAgentShadowRoot();
  return opt_group_element;
}

bool HTMLOptGroupElement::IsDisabledFormControl() const {
  return FastHasAttribute(disabledAttr);
}

void HTMLOptGroupElement::ParseAttribute(
    const AttributeModificationParams& params) {
  HTMLElement::ParseAttribute(params);

  if (params.name == disabledAttr) {
    PseudoStateChanged(CSSSelector::kPseudoDisabled);
    PseudoStateChanged(CSSSelector::kPseudoEnabled);
  } else if (params.name == labelAttr) {
    UpdateGroupLabel();
  }
}

bool HTMLOptGroupElement::SupportsFocus() const {
  HTMLSelectElement* select = OwnerSelectElement();
  if (select && select->UsesMenuList())
    return false;
  return HTMLElement::SupportsFocus();
}

bool HTMLOptGroupElement::MatchesEnabledPseudoClass() const {
  return !IsDisabledFormControl();
}

Node::InsertionNotificationRequest HTMLOptGroupElement::InsertedInto(
    ContainerNode* insertion_point) {
  HTMLElement::InsertedInto(insertion_point);
  if (HTMLSelectElement* select = OwnerSelectElement()) {
    if (insertion_point == select)
      select->OptGroupInsertedOrRemoved(*this);
  }
  return kInsertionDone;
}

void HTMLOptGroupElement::RemovedFrom(ContainerNode* insertion_point) {
  if (isHTMLSelectElement(*insertion_point)) {
    if (!parentNode())
      toHTMLSelectElement(insertion_point)->OptGroupInsertedOrRemoved(*this);
  }
  HTMLElement::RemovedFrom(insertion_point);
}

String HTMLOptGroupElement::GroupLabelText() const {
  String item_text = getAttribute(labelAttr);

  // In WinIE, leading and trailing whitespace is ignored in options and
  // optgroups. We match this behavior.
  item_text = item_text.StripWhiteSpace();
  // We want to collapse our whitespace too.  This will match other browsers.
  item_text = item_text.SimplifyWhiteSpace();

  return item_text;
}

HTMLSelectElement* HTMLOptGroupElement::OwnerSelectElement() const {
  // TODO(tkent): We should return only the parent <select>.
  return Traversal<HTMLSelectElement>::FirstAncestor(*this);
}

String HTMLOptGroupElement::DefaultToolTip() const {
  if (HTMLSelectElement* select = OwnerSelectElement())
    return select->DefaultToolTip();
  return String();
}

void HTMLOptGroupElement::AccessKeyAction(bool) {
  HTMLSelectElement* select = OwnerSelectElement();
  // send to the parent to bring focus to the list box
  if (select && !select->IsFocused())
    select->AccessKeyAction(false);
}

void HTMLOptGroupElement::DidAddUserAgentShadowRoot(ShadowRoot& root) {
  DEFINE_STATIC_LOCAL(AtomicString, label_padding, ("0 2px 1px 2px"));
  DEFINE_STATIC_LOCAL(AtomicString, label_min_height, ("1.2em"));
  HTMLDivElement* label = HTMLDivElement::Create(GetDocument());
  label->setAttribute(roleAttr, AtomicString("group"));
  label->setAttribute(aria_labelAttr, AtomicString());
  label->SetInlineStyleProperty(CSSPropertyPadding, label_padding);
  label->SetInlineStyleProperty(CSSPropertyMinHeight, label_min_height);
  label->SetIdAttribute(ShadowElementNames::OptGroupLabel());
  root.AppendChild(label);

  HTMLContentElement* content = HTMLContentElement::Create(GetDocument());
  content->setAttribute(selectAttr, "option,hr");
  root.AppendChild(content);
}

void HTMLOptGroupElement::UpdateGroupLabel() {
  const String& label_text = GroupLabelText();
  HTMLDivElement& label = OptGroupLabelElement();
  label.setTextContent(label_text);
  label.setAttribute(aria_labelAttr, AtomicString(label_text));
}

HTMLDivElement& HTMLOptGroupElement::OptGroupLabelElement() const {
  return *toHTMLDivElementOrDie(UserAgentShadowRoot()->getElementById(
      ShadowElementNames::OptGroupLabel()));
}

}  // namespace blink
