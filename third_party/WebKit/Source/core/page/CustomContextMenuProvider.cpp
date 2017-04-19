// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/page/CustomContextMenuProvider.h"

#include "core/dom/Document.h"
#include "core/dom/ElementTraversal.h"
#include "core/events/EventDispatcher.h"
#include "core/events/MouseEvent.h"
#include "core/html/HTMLMenuElement.h"
#include "core/html/HTMLMenuItemElement.h"
#include "core/page/ContextMenuController.h"
#include "core/page/Page.h"
#include "platform/ContextMenu.h"

namespace blink {

using namespace HTMLNames;

CustomContextMenuProvider::CustomContextMenuProvider(HTMLMenuElement& menu,
                                                     HTMLElement& subject)
    : menu_(menu), subject_element_(subject) {}

CustomContextMenuProvider::~CustomContextMenuProvider() {}

DEFINE_TRACE(CustomContextMenuProvider) {
  visitor->Trace(menu_);
  visitor->Trace(subject_element_);
  visitor->Trace(menu_items_);
  ContextMenuProvider::Trace(visitor);
}

void CustomContextMenuProvider::PopulateContextMenu(ContextMenu* menu) {
  PopulateContextMenuItems(*menu_, *menu);
}

void CustomContextMenuProvider::ContextMenuItemSelected(
    const ContextMenuItem* item) {
  if (HTMLElement* element = MenuItemAt(item->Action())) {
    MouseEvent* click = MouseEvent::Create(
        EventTypeNames::click, menu_->GetDocument().domWindow(),
        Event::Create(), SimulatedClickCreationScope::kFromUserAgent);
    click->SetRelatedTarget(subject_element_.Get());
    element->DispatchEvent(click);
  }
}

void CustomContextMenuProvider::ContextMenuCleared() {
  menu_items_.clear();
  subject_element_ = nullptr;
}

void CustomContextMenuProvider::AppendSeparator(ContextMenu& context_menu) {
  // Avoid separators at the start of any menu and submenu.
  if (!context_menu.Items().size())
    return;

  // Collapse all sequences of two or more adjacent separators in the menu or
  // any submenus to a single separator.
  ContextMenuItem last_item = context_menu.Items().back();
  if (last_item.GetType() == kSeparatorType)
    return;

  context_menu.AppendItem(ContextMenuItem(
      kSeparatorType, kContextMenuItemCustomTagNoAction, String(), String()));
}

void CustomContextMenuProvider::AppendMenuItem(HTMLMenuItemElement* menu_item,
                                               ContextMenu& context_menu) {
  // Avoid menuitems with no label.
  String label_string = menu_item->FastGetAttribute(labelAttr);
  if (label_string.IsEmpty())
    return;

  menu_items_.push_back(menu_item);

  bool enabled = !menu_item->FastHasAttribute(disabledAttr);
  String icon = menu_item->FastGetAttribute(iconAttr);
  if (!icon.IsEmpty()) {
    // To obtain the absolute URL of the icon when the attribute's value is not
    // the empty string, the attribute's value must be resolved relative to the
    // element.
    KURL icon_url = KURL(menu_item->baseURI(), icon);
    icon = icon_url.GetString();
  }
  ContextMenuAction action = static_cast<ContextMenuAction>(
      kContextMenuItemBaseCustomTag + menu_items_.size() - 1);
  if (DeprecatedEqualIgnoringCase(menu_item->FastGetAttribute(typeAttr),
                                  "checkbox") ||
      DeprecatedEqualIgnoringCase(menu_item->FastGetAttribute(typeAttr),
                                  "radio"))
    context_menu.AppendItem(
        ContextMenuItem(kCheckableActionType, action, label_string, icon,
                        enabled, menu_item->FastHasAttribute(checkedAttr)));
  else
    context_menu.AppendItem(ContextMenuItem(kActionType, action, label_string,
                                            icon, enabled, false));
}

void CustomContextMenuProvider::PopulateContextMenuItems(
    const HTMLMenuElement& menu,
    ContextMenu& context_menu) {
  HTMLElement* next_element = Traversal<HTMLElement>::FirstWithin(menu);
  while (next_element) {
    if (isHTMLHRElement(*next_element)) {
      AppendSeparator(context_menu);
      next_element = Traversal<HTMLElement>::Next(*next_element, &menu);
    } else if (isHTMLMenuElement(*next_element)) {
      ContextMenu sub_menu;
      String label_string = next_element->FastGetAttribute(labelAttr);
      if (label_string.IsNull()) {
        AppendSeparator(context_menu);
        PopulateContextMenuItems(*toHTMLMenuElement(next_element),
                                 context_menu);
        AppendSeparator(context_menu);
      } else if (!label_string.IsEmpty()) {
        PopulateContextMenuItems(*toHTMLMenuElement(next_element), sub_menu);
        context_menu.AppendItem(
            ContextMenuItem(kSubmenuType, kContextMenuItemCustomTagNoAction,
                            label_string, String(), &sub_menu));
      }
      next_element = Traversal<HTMLElement>::NextSibling(*next_element);
    } else if (isHTMLMenuItemElement(*next_element)) {
      AppendMenuItem(toHTMLMenuItemElement(next_element), context_menu);
      if (kContextMenuItemBaseCustomTag + menu_items_.size() >=
          kContextMenuItemLastCustomTag)
        break;
      next_element = Traversal<HTMLElement>::Next(*next_element, &menu);
    } else {
      next_element = Traversal<HTMLElement>::Next(*next_element, &menu);
    }
  }

  // Remove separators at the end of the menu and any submenus.
  while (context_menu.Items().size() &&
         context_menu.Items().back().GetType() == kSeparatorType)
    context_menu.RemoveLastItem();
}

HTMLElement* CustomContextMenuProvider::MenuItemAt(unsigned menu_id) {
  int item_index = menu_id - kContextMenuItemBaseCustomTag;
  if (item_index < 0 ||
      static_cast<unsigned long>(item_index) >= menu_items_.size())
    return nullptr;
  return menu_items_[item_index].Get();
}

}  // namespace blink
