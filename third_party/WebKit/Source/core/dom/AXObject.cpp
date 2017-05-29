// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/AXObject.h"

#include "core/HTMLElementTypeHelpers.h"
#include "core/dom/Element.h"
#include "core/dom/Node.h"
#include "platform/wtf/HashSet.h"
#include "platform/wtf/text/StringHash.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

namespace {

typedef HashSet<String, CaseFoldingHash> ARIAWidgetSet;

const char* g_aria_widgets[] = {
    // From http://www.w3.org/TR/wai-aria/roles#widget_roles
    "alert", "alertdialog", "button", "checkbox", "dialog", "gridcell", "link",
    "log", "marquee", "menuitem", "menuitemcheckbox", "menuitemradio", "option",
    "progressbar", "radio", "scrollbar", "slider", "spinbutton", "status",
    "tab", "tabpanel", "textbox", "timer", "tooltip", "treeitem",
    // Composite user interface widgets.
    // This list is also from the w3.org site referenced above.
    "combobox", "grid", "listbox", "menu", "menubar", "radiogroup", "tablist",
    "tree", "treegrid"};

static ARIAWidgetSet* CreateARIARoleWidgetSet() {
  ARIAWidgetSet* widget_set = new HashSet<String, CaseFoldingHash>();
  for (size_t i = 0; i < WTF_ARRAY_LENGTH(g_aria_widgets); ++i)
    widget_set->insert(String(g_aria_widgets[i]));
  return widget_set;
}

bool IncludesARIAWidgetRole(const String& role) {
  static const HashSet<String, CaseFoldingHash>* role_set =
      CreateARIARoleWidgetSet();

  Vector<String> role_vector;
  role.Split(' ', role_vector);
  for (const auto& child : role_vector) {
    if (role_set->Contains(child))
      return true;
  }
  return false;
}

const char* g_aria_interactive_widget_attributes[] = {
    // These attributes implicitly indicate the given widget is interactive.
    // From http://www.w3.org/TR/wai-aria/states_and_properties#attrs_widgets
    // clang-format off
    "aria-activedescendant",
    "aria-checked",
    "aria-controls",
    "aria-disabled",  // If it's disabled, it can be made interactive.
    "aria-expanded",
    "aria-haspopup",
    "aria-multiselectable",
    "aria-pressed",
    "aria-required",
    "aria-selected"
    // clang-format on
};

bool HasInteractiveARIAAttribute(const Element& element) {
  for (size_t i = 0; i < WTF_ARRAY_LENGTH(g_aria_interactive_widget_attributes);
       ++i) {
    const char* attribute = g_aria_interactive_widget_attributes[i];
    if (element.hasAttribute(attribute)) {
      return true;
    }
  }
  return false;
}

}  // namespace

bool AXObject::IsInsideFocusableElementOrARIAWidget(const Node& node) {
  const Node* cur_node = &node;
  do {
    if (cur_node->IsElementNode()) {
      const Element* element = ToElement(cur_node);
      if (element->IsFocusable())
        return true;
      String role = element->getAttribute("role");
      if (!role.IsEmpty() && IncludesARIAWidgetRole(role))
        return true;
      if (HasInteractiveARIAAttribute(*element))
        return true;
    }
    cur_node = cur_node->parentNode();
  } while (cur_node && !isHTMLBodyElement(node));
  return false;
}

}  // namespace blink
