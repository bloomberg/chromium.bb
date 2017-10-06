/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "modules/accessibility/AXListBoxOption.h"

#include "core/dom/AccessibleNode.h"
#include "core/html/forms/HTMLOptionElement.h"
#include "core/html/forms/HTMLSelectElement.h"
#include "core/layout/LayoutObject.h"
#include "modules/accessibility/AXObjectCacheImpl.h"

namespace blink {

using namespace HTMLNames;

AXListBoxOption::AXListBoxOption(LayoutObject* layout_object,
                                 AXObjectCacheImpl& ax_object_cache)
    : AXLayoutObject(layout_object, ax_object_cache) {}

AXListBoxOption::~AXListBoxOption() {}

AXListBoxOption* AXListBoxOption::Create(LayoutObject* layout_object,
                                         AXObjectCacheImpl& ax_object_cache) {
  return new AXListBoxOption(layout_object, ax_object_cache);
}

AccessibilityRole AXListBoxOption::DetermineAccessibilityRole() {
  if ((aria_role_ = DetermineAriaRoleAttribute()) != kUnknownRole)
    return aria_role_;

  // http://www.w3.org/TR/wai-aria/complete#presentation
  // ARIA spec says that the presentation role causes a given element to be
  // treated as having no role or to be removed from the accessibility tree, but
  // does not cause the content contained within the element to be removed from
  // the accessibility tree.
  if (IsParentPresentationalRole())
    return kStaticTextRole;

  return kListBoxOptionRole;
}

bool AXListBoxOption::IsParentPresentationalRole() const {
  AXObject* parent = ParentObject();
  if (!parent)
    return false;

  LayoutObject* layout_object = parent->GetLayoutObject();
  if (!layout_object)
    return false;

  if (layout_object->IsListBox() && parent->HasInheritedPresentationalRole())
    return true;

  return false;
}

bool AXListBoxOption::IsSelected() const {
  return IsHTMLOptionElement(GetNode()) &&
         ToHTMLOptionElement(GetNode())->Selected();
}

bool AXListBoxOption::IsSelectedOptionActive() const {
  HTMLSelectElement* list_box_parent_node = ListBoxOptionParentNode();
  if (!list_box_parent_node)
    return false;

  return list_box_parent_node->ActiveSelectionEnd() == GetNode();
}

bool AXListBoxOption::ComputeAccessibilityIsIgnored(
    IgnoredReasons* ignored_reasons) const {
  if (!GetNode())
    return true;

  if (AccessibilityIsIgnoredByDefault(ignored_reasons))
    return true;

  return false;
}

String AXListBoxOption::TextAlternative(bool recursive,
                                        bool in_aria_labelled_by_traversal,
                                        AXObjectSet& visited,
                                        AXNameFrom& name_from,
                                        AXRelatedObjectVector* related_objects,
                                        NameSources* name_sources) const {
  // If nameSources is non-null, relatedObjects is used in filling it in, so it
  // must be non-null as well.
  if (name_sources)
    DCHECK(related_objects);

  if (!GetNode())
    return String();

  bool found_text_alternative = false;
  String text_alternative = AriaTextAlternative(
      recursive, in_aria_labelled_by_traversal, visited, name_from,
      related_objects, name_sources, &found_text_alternative);
  if (found_text_alternative && !name_sources)
    return text_alternative;

  name_from = kAXNameFromContents;
  text_alternative = ToHTMLOptionElement(GetNode())->DisplayLabel();
  if (name_sources) {
    name_sources->push_back(NameSource(found_text_alternative));
    name_sources->back().type = name_from;
    name_sources->back().text = text_alternative;
    found_text_alternative = true;
  }

  return text_alternative;
}

bool AXListBoxOption::OnNativeSetSelectedAction(bool selected) {
  HTMLSelectElement* select_element = ListBoxOptionParentNode();
  if (!select_element)
    return false;

  if (!CanSetSelectedAttribute())
    return false;

  bool is_option_selected = IsSelected();
  if ((is_option_selected && selected) || (!is_option_selected && !selected))
    return false;

  select_element->SelectOptionByAccessKey(ToHTMLOptionElement(GetNode()));
  return true;
}

HTMLSelectElement* AXListBoxOption::ListBoxOptionParentNode() const {
  if (!GetNode())
    return 0;

  if (auto* option = ToHTMLOptionElementOrNull(GetNode()))
    return option->OwnerSelectElement();

  return 0;
}

}  // namespace blink
