/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2010 Igalia S.L
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ContextMenuItem_h
#define ContextMenuItem_h

#include "platform/PlatformExport.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class ContextMenu;

enum ContextMenuAction {
  kContextMenuItemBaseCustomTag = 5000,
  kContextMenuItemCustomTagNoAction = 5998,
  kContextMenuItemLastCustomTag = 5999
};

enum ContextMenuItemType {
  kActionType,
  kCheckableActionType,
  kSeparatorType,
  kSubmenuType
};

class PLATFORM_EXPORT ContextMenuItem {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

 public:
  ContextMenuItem(ContextMenuItemType,
                  ContextMenuAction,
                  const String& title,
                  ContextMenu* sub_menu = nullptr);
  ContextMenuItem(ContextMenuItemType,
                  ContextMenuAction,
                  const String& title,
                  bool enabled,
                  bool checked);

  ~ContextMenuItem();

  void SetType(ContextMenuItemType);
  ContextMenuItemType GetType() const;

  void SetAction(ContextMenuAction);
  ContextMenuAction Action() const;

  void SetChecked(bool = true);
  bool Checked() const;

  void SetEnabled(bool = true);
  bool Enabled() const;

  void SetSubMenu(ContextMenu*);

  ContextMenuItem(ContextMenuAction,
                  const String&,
                  bool enabled,
                  bool checked,
                  const Vector<ContextMenuItem>& sub_menu_items);

  void SetTitle(const String& title) { title_ = title; }
  const String& Title() const { return title_; }

  const Vector<ContextMenuItem>& SubMenuItems() const {
    return sub_menu_items_;
  }

 private:
  ContextMenuItemType type_;
  ContextMenuAction action_;
  String title_;
  bool enabled_;
  bool checked_;
  Vector<ContextMenuItem> sub_menu_items_;
};

}  // namespace blink

#endif  // ContextMenuItem_h
