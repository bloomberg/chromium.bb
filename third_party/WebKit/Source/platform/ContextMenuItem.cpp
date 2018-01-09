/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/ContextMenuItem.h"

#include "platform/ContextMenu.h"

namespace blink {

ContextMenuItem::ContextMenuItem(ContextMenuItemType type,
                                 ContextMenuAction action,
                                 const String& title,
                                 ContextMenu* sub_menu)
    : type_(type),
      action_(action),
      title_(title),
      enabled_(true),
      checked_(false) {
  if (sub_menu)
    SetSubMenu(sub_menu);
}

ContextMenuItem::ContextMenuItem(ContextMenuItemType type,
                                 ContextMenuAction action,
                                 const String& title,
                                 bool enabled,
                                 bool checked)
    : type_(type),
      action_(action),
      title_(title),
      enabled_(enabled),
      checked_(checked) {}

ContextMenuItem::ContextMenuItem(ContextMenuAction action,
                                 const String& title,
                                 bool enabled,
                                 bool checked,
                                 const Vector<ContextMenuItem>& sub_menu_items)
    : type_(kSubmenuType),
      action_(action),
      title_(title),
      enabled_(enabled),
      checked_(checked),
      sub_menu_items_(sub_menu_items) {}

ContextMenuItem::~ContextMenuItem() = default;

void ContextMenuItem::SetSubMenu(ContextMenu* sub_menu) {
  if (sub_menu) {
    type_ = kSubmenuType;
    sub_menu_items_ = sub_menu->Items();
  } else {
    type_ = kActionType;
    sub_menu_items_.clear();
  }
}

void ContextMenuItem::SetType(ContextMenuItemType type) {
  type_ = type;
}

ContextMenuItemType ContextMenuItem::GetType() const {
  return type_;
}

void ContextMenuItem::SetAction(ContextMenuAction action) {
  action_ = action;
}

ContextMenuAction ContextMenuItem::Action() const {
  return action_;
}

void ContextMenuItem::SetChecked(bool checked) {
  checked_ = checked;
}

bool ContextMenuItem::Checked() const {
  return checked_;
}

void ContextMenuItem::SetEnabled(bool enabled) {
  enabled_ = enabled;
}

bool ContextMenuItem::Enabled() const {
  return enabled_;
}

}  // namespace blink
