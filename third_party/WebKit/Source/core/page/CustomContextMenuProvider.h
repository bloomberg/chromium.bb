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

class CustomContextMenuProvider FINAL : public ContextMenuProvider {
public:
    static PassRefPtr<CustomContextMenuProvider> create(HTMLMenuElement& menu, HTMLElement& subject)
    {
        return adoptRef(new CustomContextMenuProvider(menu, subject));
    }

private:
    CustomContextMenuProvider(HTMLMenuElement&, HTMLElement&);
    virtual ~CustomContextMenuProvider();

    virtual void populateContextMenu(ContextMenu*) OVERRIDE;
    virtual void contextMenuItemSelected(const ContextMenuItem*) OVERRIDE;
    virtual void contextMenuCleared() OVERRIDE;
    void populateContextMenuItems(const HTMLMenuElement&, ContextMenu&);
    void appendSeparator(ContextMenu&);
    void appendMenuItem(HTMLMenuItemElement*, ContextMenu&);
    HTMLElement* menuItemAt(unsigned menuId);

    RefPtrWillBePersistent<HTMLMenuElement> m_menu;
    RefPtrWillBePersistent<HTMLElement> m_subjectElement;
    WillBePersistentHeapVector<RefPtrWillBeMember<HTMLElement> > m_menuItems;
};

} // namespace blink

#endif
