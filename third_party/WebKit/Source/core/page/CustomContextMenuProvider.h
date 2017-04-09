// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CustomContextMenuProvider_h
#define CustomContextMenuProvider_h

#include "core/page/ContextMenuProvider.h"
#include "platform/ContextMenuItem.h"
#include "platform/heap/Handle.h"

namespace blink {

class ContextMenu;
class HTMLElement;
class HTMLMenuElement;
class HTMLMenuItemElement;

class CustomContextMenuProvider final : public ContextMenuProvider {
 public:
  ~CustomContextMenuProvider() override;

  static CustomContextMenuProvider* Create(HTMLMenuElement& menu,
                                           HTMLElement& subject) {
    return new CustomContextMenuProvider(menu, subject);
  }

  DECLARE_VIRTUAL_TRACE();

 private:
  CustomContextMenuProvider(HTMLMenuElement&, HTMLElement&);

  void PopulateContextMenu(ContextMenu*) override;
  void ContextMenuItemSelected(const ContextMenuItem*) override;
  void ContextMenuCleared() override;
  void PopulateContextMenuItems(const HTMLMenuElement&, ContextMenu&);
  void AppendSeparator(ContextMenu&);
  void AppendMenuItem(HTMLMenuItemElement*, ContextMenu&);
  HTMLElement* MenuItemAt(unsigned menu_id);

  Member<HTMLMenuElement> menu_;
  Member<HTMLElement> subject_element_;
  HeapVector<Member<HTMLElement>> menu_items_;
};

}  // namespace blink

#endif
