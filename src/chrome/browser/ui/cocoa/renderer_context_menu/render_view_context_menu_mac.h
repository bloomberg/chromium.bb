// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_RENDERER_CONTEXT_MENU_RENDER_VIEW_CONTEXT_MENU_MAC_H_
#define CHROME_BROWSER_UI_COCOA_RENDERER_CONTEXT_MENU_RENDER_VIEW_CONTEXT_MENU_MAC_H_

#import <Cocoa/Cocoa.h>

#include "base/macros.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"
#include "ui/base/cocoa/text_services_context_menu.h"

// Mac implementation of the context menu display code. Uses a Cocoa NSMenu
// to display the context menu. Internally uses an obj-c object as the
// target of the NSMenu, bridging back to this C++ class.
class RenderViewContextMenuMac : public RenderViewContextMenu,
                                 public ui::TextServicesContextMenu::Delegate {
 public:
  RenderViewContextMenuMac(content::RenderFrameHost* render_frame_host,
                           const content::ContextMenuParams& params);
  ~RenderViewContextMenuMac() override;

  // SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override;
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;

  // TextServicesContextMenu::Delegate:
  base::string16 GetSelectedText() const override;
  bool IsTextDirectionEnabled(
      base::i18n::TextDirection direction) const override;
  bool IsTextDirectionChecked(
      base::i18n::TextDirection direction) const override;
  void UpdateTextDirection(base::i18n::TextDirection direction) override;

 protected:
  // Adds menu to the platform's toolkit.
  void InitToolkitMenu();

  // RenderViewContextMenu:
  void AppendPlatformEditableItems() override;

 private:
  // Handler for the "Look Up" menu item.
  void LookUpInDictionary();

  // Returns the ContextMenuParams value associated with |direction|.
  int ParamsForTextDirection(base::i18n::TextDirection direction) const;

  // The context menu that adds and handles Speech and BiDi.
  ui::TextServicesContextMenu text_services_context_menu_;

  DISALLOW_COPY_AND_ASSIGN(RenderViewContextMenuMac);
};

#endif  // CHROME_BROWSER_UI_COCOA_RENDERER_CONTEXT_MENU_RENDER_VIEW_CONTEXT_MENU_MAC_H_
