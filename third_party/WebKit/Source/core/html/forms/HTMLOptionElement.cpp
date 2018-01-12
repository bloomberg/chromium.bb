/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2006 Alexey Proskuryakov (ap@nypop.com)
 * Copyright (C) 2004, 2005, 2006, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2011 Motorola Mobility, Inc.  All rights reserved.
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

#include "core/html/forms/HTMLOptionElement.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/AXObjectCache.h"
#include "core/dom/Document.h"
#include "core/dom/NodeComputedStyle.h"
#include "core/dom/NodeTraversal.h"
#include "core/dom/ShadowRoot.h"
#include "core/dom/Text.h"
#include "core/html/forms/HTMLDataListElement.h"
#include "core/html/forms/HTMLOptGroupElement.h"
#include "core/html/forms/HTMLSelectElement.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/html_names.h"
#include "core/layout/LayoutTheme.h"
#include "core/style/ComputedStyle.h"
#include "platform/wtf/text/StringBuilder.h"

namespace blink {

using namespace HTMLNames;

HTMLOptionElement::HTMLOptionElement(Document& document)
    : HTMLElement(optionTag, document), is_selected_(false) {}

// An explicit empty destructor should be in HTMLOptionElement.cpp, because
// if an implicit destructor is used or an empty destructor is defined in
// HTMLOptionElement.h, when including HTMLOptionElement.h,
// msvc tries to expand the destructor and causes
// a compile error because of lack of ComputedStyle definition.
HTMLOptionElement::~HTMLOptionElement() = default;

HTMLOptionElement* HTMLOptionElement::Create(Document& document) {
  HTMLOptionElement* option = new HTMLOptionElement(document);
  option->EnsureUserAgentShadowRootV1();
  return option;
}

HTMLOptionElement* HTMLOptionElement::CreateForJSConstructor(
    Document& document,
    const String& data,
    const AtomicString& value,
    bool default_selected,
    bool selected,
    ExceptionState& exception_state) {
  HTMLOptionElement* element = new HTMLOptionElement(document);
  element->EnsureUserAgentShadowRootV1();
  if (!data.IsEmpty()) {
    element->AppendChild(Text::Create(document, data), exception_state);
    if (exception_state.HadException())
      return nullptr;
  }

  if (!value.IsNull())
    element->setValue(value);
  if (default_selected)
    element->setAttribute(selectedAttr, g_empty_atom);
  element->SetSelected(selected);

  return element;
}

void HTMLOptionElement::AttachLayoutTree(AttachContext& context) {
  AttachContext option_context(context);
  if (!GetNonAttachedStyle() && ParentComputedStyle()) {
    if (HTMLSelectElement* select = OwnerSelectElement())
      select->UpdateListOnLayoutObject();
    SetNonAttachedStyle(OriginalStyleForLayoutObject());
  }
  HTMLElement::AttachLayoutTree(option_context);
}

bool HTMLOptionElement::SupportsFocus() const {
  HTMLSelectElement* select = OwnerSelectElement();
  if (select && select->UsesMenuList())
    return false;
  return HTMLElement::SupportsFocus();
}

bool HTMLOptionElement::MatchesDefaultPseudoClass() const {
  return FastHasAttribute(selectedAttr);
}

bool HTMLOptionElement::MatchesEnabledPseudoClass() const {
  return !IsDisabledFormControl();
}

String HTMLOptionElement::DisplayLabel() const {
  Document& document = GetDocument();
  String text;

  // WinIE does not use the label attribute, so as a quirk, we ignore it.
  if (!document.InQuirksMode())
    text = FastGetAttribute(labelAttr);

  // FIXME: The following treats an element with the label attribute set to
  // the empty string the same as an element with no label attribute at all.
  // Is that correct? If it is, then should the label function work the same
  // way?
  if (text.IsEmpty())
    text = CollectOptionInnerText();

  return text.StripWhiteSpace(IsHTMLSpace<UChar>)
      .SimplifyWhiteSpace(IsHTMLSpace<UChar>);
}

String HTMLOptionElement::text() const {
  return CollectOptionInnerText()
      .StripWhiteSpace(IsHTMLSpace<UChar>)
      .SimplifyWhiteSpace(IsHTMLSpace<UChar>);
}

void HTMLOptionElement::setText(const String& text) {
  // Changing the text causes a recalc of a select's items, which will reset the
  // selected index to the first item if the select is single selection with a
  // menu list.  We attempt to preserve the selected item.
  HTMLSelectElement* select = OwnerSelectElement();
  bool select_is_menu_list = select && select->UsesMenuList();
  int old_selected_index = select_is_menu_list ? select->selectedIndex() : -1;

  setTextContent(text);

  if (select_is_menu_list && select->selectedIndex() != old_selected_index)
    select->setSelectedIndex(old_selected_index);
}

void HTMLOptionElement::AccessKeyAction(bool) {
  if (HTMLSelectElement* select = OwnerSelectElement())
    select->SelectOptionByAccessKey(this);
}

int HTMLOptionElement::index() const {
  // It would be faster to cache the index, but harder to get it right in all
  // cases.

  HTMLSelectElement* select_element = OwnerSelectElement();
  if (!select_element)
    return 0;

  int option_index = 0;
  for (auto* const option : select_element->GetOptionList()) {
    if (option == this)
      return option_index;
    ++option_index;
  }

  return 0;
}

int HTMLOptionElement::ListIndex() const {
  if (HTMLSelectElement* select_element = OwnerSelectElement())
    return select_element->ListIndexForOption(*this);
  return -1;
}

void HTMLOptionElement::ParseAttribute(
    const AttributeModificationParams& params) {
  const QualifiedName& name = params.name;
  if (name == valueAttr) {
    if (HTMLDataListElement* data_list = OwnerDataListElement())
      data_list->OptionElementChildrenChanged();
  } else if (name == disabledAttr) {
    if (params.old_value.IsNull() != params.new_value.IsNull()) {
      PseudoStateChanged(CSSSelector::kPseudoDisabled);
      PseudoStateChanged(CSSSelector::kPseudoEnabled);
      if (LayoutObject* o = GetLayoutObject())
        o->InvalidateIfControlStateChanged(kEnabledControlState);
    }
  } else if (name == selectedAttr) {
    if (params.old_value.IsNull() != params.new_value.IsNull() && !is_dirty_)
      SetSelected(!params.new_value.IsNull());
    PseudoStateChanged(CSSSelector::kPseudoDefault);
  } else if (name == labelAttr) {
    UpdateLabel();
  } else {
    HTMLElement::ParseAttribute(params);
  }
}

String HTMLOptionElement::value() const {
  const AtomicString& value = FastGetAttribute(valueAttr);
  if (!value.IsNull())
    return value;
  return CollectOptionInnerText()
      .StripWhiteSpace(IsHTMLSpace<UChar>)
      .SimplifyWhiteSpace(IsHTMLSpace<UChar>);
}

void HTMLOptionElement::setValue(const AtomicString& value) {
  setAttribute(valueAttr, value);
}

bool HTMLOptionElement::Selected() const {
  return is_selected_;
}

void HTMLOptionElement::SetSelected(bool selected) {
  if (is_selected_ == selected)
    return;

  SetSelectedState(selected);

  if (HTMLSelectElement* select = OwnerSelectElement())
    select->OptionSelectionStateChanged(this, selected);
}

bool HTMLOptionElement::selectedForBinding() const {
  return Selected();
}

void HTMLOptionElement::setSelectedForBinding(bool selected) {
  bool was_selected = is_selected_;
  SetSelected(selected);

  // As of December 2015, the HTML specification says the dirtiness becomes
  // true by |selected| setter unconditionally. However it caused a real bug,
  // crbug.com/570367, and is not compatible with other browsers.
  // Firefox seems not to set dirtiness if an option is owned by a select
  // element and selectedness is not changed.
  if (OwnerSelectElement() && was_selected == is_selected_)
    return;

  is_dirty_ = true;
}

void HTMLOptionElement::SetSelectedState(bool selected) {
  if (is_selected_ == selected)
    return;

  is_selected_ = selected;
  PseudoStateChanged(CSSSelector::kPseudoChecked);

  if (HTMLSelectElement* select = OwnerSelectElement()) {
    select->InvalidateSelectedItems();

    if (AXObjectCache* cache = GetDocument().ExistingAXObjectCache()) {
      // If there is a layoutObject (most common), fire accessibility
      // notifications only when it's a listbox (and not a menu list). If
      // there's no layoutObject, fire them anyway just to be safe (to make sure
      // the AX tree is in sync).
      if (!select->GetLayoutObject() ||
          select->GetLayoutObject()->IsListBox()) {
        cache->ListboxOptionStateChanged(this);
        cache->ListboxSelectedChildrenChanged(select);
      }
    }
  }
}

void HTMLOptionElement::SetDirty(bool value) {
  is_dirty_ = value;
}

void HTMLOptionElement::ChildrenChanged(const ChildrenChange& change) {
  if (HTMLDataListElement* data_list = OwnerDataListElement())
    data_list->OptionElementChildrenChanged();
  else if (HTMLSelectElement* select = OwnerSelectElement())
    select->OptionElementChildrenChanged(*this);
  UpdateLabel();
  HTMLElement::ChildrenChanged(change);
}

HTMLDataListElement* HTMLOptionElement::OwnerDataListElement() const {
  return Traversal<HTMLDataListElement>::FirstAncestor(*this);
}

HTMLSelectElement* HTMLOptionElement::OwnerSelectElement() const {
  if (!parentNode())
    return nullptr;
  if (auto* select = ToHTMLSelectElementOrNull(*parentNode()))
    return select;
  if (IsHTMLOptGroupElement(*parentNode()))
    return ToHTMLSelectElementOrNull(parentNode()->parentNode());
  return nullptr;
}

String HTMLOptionElement::label() const {
  const AtomicString& label = FastGetAttribute(labelAttr);
  if (!label.IsNull())
    return label;
  return CollectOptionInnerText()
      .StripWhiteSpace(IsHTMLSpace<UChar>)
      .SimplifyWhiteSpace(IsHTMLSpace<UChar>);
}

void HTMLOptionElement::setLabel(const AtomicString& label) {
  setAttribute(labelAttr, label);
}

String HTMLOptionElement::TextIndentedToRespectGroupLabel() const {
  ContainerNode* parent = parentNode();
  if (parent && IsHTMLOptGroupElement(*parent))
    return "    " + DisplayLabel();
  return DisplayLabel();
}

bool HTMLOptionElement::OwnElementDisabled() const {
  return FastHasAttribute(disabledAttr);
}

bool HTMLOptionElement::IsDisabledFormControl() const {
  if (OwnElementDisabled())
    return true;
  if (Element* parent = parentElement())
    return IsHTMLOptGroupElement(*parent) && parent->IsDisabledFormControl();
  return false;
}

String HTMLOptionElement::DefaultToolTip() const {
  if (HTMLSelectElement* select = OwnerSelectElement())
    return select->DefaultToolTip();
  return String();
}

Node::InsertionNotificationRequest HTMLOptionElement::InsertedInto(
    ContainerNode* insertion_point) {
  HTMLElement::InsertedInto(insertion_point);
  if (HTMLSelectElement* select = OwnerSelectElement()) {
    if (insertion_point == select || (IsHTMLOptGroupElement(*insertion_point) &&
                                      insertion_point->parentNode() == select))
      select->OptionInserted(*this, is_selected_);
  }
  return kInsertionDone;
}

void HTMLOptionElement::RemovedFrom(ContainerNode* insertion_point) {
  if (auto* select = ToHTMLSelectElementOrNull(*insertion_point)) {
    if (!parentNode() || IsHTMLOptGroupElement(*parentNode()))
      select->OptionRemoved(*this);
  } else if (IsHTMLOptGroupElement(*insertion_point)) {
    if (auto* select = ToHTMLSelectElementOrNull(insertion_point->parentNode()))
      select->OptionRemoved(*this);
  }
  HTMLElement::RemovedFrom(insertion_point);
}

String HTMLOptionElement::CollectOptionInnerText() const {
  StringBuilder text;
  for (Node* node = firstChild(); node;) {
    if (node->IsTextNode())
      text.Append(node->nodeValue());
    // Text nodes inside script elements are not part of the option text.
    if (node->IsElementNode() && ToElement(node)->IsScriptElement())
      node = NodeTraversal::NextSkippingChildren(*node, this);
    else
      node = NodeTraversal::Next(*node, this);
  }
  return text.ToString();
}

HTMLFormElement* HTMLOptionElement::form() const {
  if (HTMLSelectElement* select_element = OwnerSelectElement())
    return select_element->formOwner();

  return nullptr;
}

void HTMLOptionElement::DidAddUserAgentShadowRoot(ShadowRoot& root) {
  UpdateLabel();
}

void HTMLOptionElement::UpdateLabel() {
  if (ShadowRoot* root = UserAgentShadowRoot())
    root->setTextContent(DisplayLabel());
}

bool HTMLOptionElement::SpatialNavigationFocused() const {
  HTMLSelectElement* select = OwnerSelectElement();
  if (!select || !select->IsFocused())
    return false;
  return select->SpatialNavigationFocusedOption() == this;
}

bool HTMLOptionElement::IsDisplayNone() const {
  // If the style is not set, then the node is still unattached.
  // We have to wait till it gets attached to read the display property.
  const ComputedStyle* style = NonLayoutObjectComputedStyle();
  if (!style)
    return false;

  if (style->Display() != EDisplay::kNone) {
    // We need to check the parent's display property.  Parent's
    // display:none doesn't override children's display properties in
    // ComputedStyle.
    Element* parent = parentElement();
    DCHECK(parent);
    if (IsHTMLOptGroupElement(*parent)) {
      const ComputedStyle* parent_style = parent->GetComputedStyle()
                                              ? parent->GetComputedStyle()
                                              : parent->EnsureComputedStyle();
      return !parent_style || parent_style->Display() == EDisplay::kNone;
    }
  }
  return style->Display() == EDisplay::kNone;
}

String HTMLOptionElement::innerText() {
  // A workaround for crbug.com/424578. We add ShadowRoot to an OPTION, but
  // innerText behavior for Shadow DOM is unclear.  We just return the same
  // string before adding ShadowRoot.
  return textContent();
}

}  // namespace blink
