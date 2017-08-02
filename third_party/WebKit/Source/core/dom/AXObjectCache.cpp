/*
 * Copyright (C) 2008, 2009, 2010 Apple Inc. All rights reserved.
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

#include "core/dom/AXObjectCache.h"

#include <memory>
#include "core/HTMLElementTypeHelpers.h"
#include "core/dom/Element.h"
#include "core/dom/Node.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/HashSet.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/text/StringHash.h"
#include "platform/wtf/text/WTFString.h"
#include "public/web/WebAXEnums.h"

namespace blink {

AXObjectCache::AXObjectCacheCreateFunction AXObjectCache::create_function_ =
    nullptr;

void AXObjectCache::Init(AXObjectCacheCreateFunction function) {
  DCHECK(!create_function_);
  create_function_ = function;
}

AXObjectCache* AXObjectCache::Create(Document& document) {
  DCHECK(create_function_);
  return create_function_(document);
}

AXObjectCache::AXObjectCache() {}

AXObjectCache::~AXObjectCache() {}

std::unique_ptr<ScopedAXObjectCache> ScopedAXObjectCache::Create(
    Document& document) {
  return WTF::WrapUnique(new ScopedAXObjectCache(document));
}

ScopedAXObjectCache::ScopedAXObjectCache(Document& document)
    : document_(&document) {
  if (!document_->AxObjectCache())
    cache_ = AXObjectCache::Create(*document_);
}

ScopedAXObjectCache::~ScopedAXObjectCache() {
  if (cache_)
    cache_->Dispose();
}

AXObjectCache* ScopedAXObjectCache::Get() {
  if (cache_)
    return cache_.Get();
  AXObjectCache* cache = document_->AxObjectCache();
  DCHECK(cache);
  return cache;
}

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

bool AXObjectCache::IsInsideFocusableElementOrARIAWidget(const Node& node) {
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

STATIC_ASSERT_ENUM(kWebAXEventActiveDescendantChanged,
                   AXObjectCache::kAXActiveDescendantChanged);
STATIC_ASSERT_ENUM(kWebAXEventAlert, AXObjectCache::kAXAlert);
STATIC_ASSERT_ENUM(kWebAXEventAriaAttributeChanged,
                   AXObjectCache::kAXAriaAttributeChanged);
STATIC_ASSERT_ENUM(kWebAXEventAutocorrectionOccured,
                   AXObjectCache::kAXAutocorrectionOccured);
STATIC_ASSERT_ENUM(kWebAXEventBlur, AXObjectCache::kAXBlur);
STATIC_ASSERT_ENUM(kWebAXEventCheckedStateChanged,
                   AXObjectCache::kAXCheckedStateChanged);
STATIC_ASSERT_ENUM(kWebAXEventChildrenChanged,
                   AXObjectCache::kAXChildrenChanged);
STATIC_ASSERT_ENUM(kWebAXEventClicked, AXObjectCache::kAXClicked);
STATIC_ASSERT_ENUM(kWebAXEventDocumentSelectionChanged,
                   AXObjectCache::kAXDocumentSelectionChanged);
STATIC_ASSERT_ENUM(kWebAXEventExpandedChanged,
                   AXObjectCache::kAXExpandedChanged);
STATIC_ASSERT_ENUM(kWebAXEventFocus, AXObjectCache::kAXFocusedUIElementChanged);
STATIC_ASSERT_ENUM(kWebAXEventHide, AXObjectCache::kAXHide);
STATIC_ASSERT_ENUM(kWebAXEventHover, AXObjectCache::kAXHover);
STATIC_ASSERT_ENUM(kWebAXEventInvalidStatusChanged,
                   AXObjectCache::kAXInvalidStatusChanged);
STATIC_ASSERT_ENUM(kWebAXEventLayoutComplete, AXObjectCache::kAXLayoutComplete);
STATIC_ASSERT_ENUM(kWebAXEventLiveRegionChanged,
                   AXObjectCache::kAXLiveRegionChanged);
STATIC_ASSERT_ENUM(kWebAXEventLoadComplete, AXObjectCache::kAXLoadComplete);
STATIC_ASSERT_ENUM(kWebAXEventLocationChanged,
                   AXObjectCache::kAXLocationChanged);
STATIC_ASSERT_ENUM(kWebAXEventMenuListItemSelected,
                   AXObjectCache::kAXMenuListItemSelected);
STATIC_ASSERT_ENUM(kWebAXEventMenuListItemUnselected,
                   AXObjectCache::kAXMenuListItemUnselected);
STATIC_ASSERT_ENUM(kWebAXEventMenuListValueChanged,
                   AXObjectCache::kAXMenuListValueChanged);
STATIC_ASSERT_ENUM(kWebAXEventRowCollapsed, AXObjectCache::kAXRowCollapsed);
STATIC_ASSERT_ENUM(kWebAXEventRowCountChanged,
                   AXObjectCache::kAXRowCountChanged);
STATIC_ASSERT_ENUM(kWebAXEventRowExpanded, AXObjectCache::kAXRowExpanded);
STATIC_ASSERT_ENUM(kWebAXEventScrollPositionChanged,
                   AXObjectCache::kAXScrollPositionChanged);
STATIC_ASSERT_ENUM(kWebAXEventScrolledToAnchor,
                   AXObjectCache::kAXScrolledToAnchor);
STATIC_ASSERT_ENUM(kWebAXEventSelectedChildrenChanged,
                   AXObjectCache::kAXSelectedChildrenChanged);
STATIC_ASSERT_ENUM(kWebAXEventSelectedTextChanged,
                   AXObjectCache::kAXSelectedTextChanged);
STATIC_ASSERT_ENUM(kWebAXEventShow, AXObjectCache::kAXShow);
STATIC_ASSERT_ENUM(kWebAXEventTextChanged, AXObjectCache::kAXTextChanged);
STATIC_ASSERT_ENUM(kWebAXEventTextInserted, AXObjectCache::kAXTextInserted);
STATIC_ASSERT_ENUM(kWebAXEventTextRemoved, AXObjectCache::kAXTextRemoved);
STATIC_ASSERT_ENUM(kWebAXEventValueChanged, AXObjectCache::kAXValueChanged);

}  // namespace blink
