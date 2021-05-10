// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_ITEM_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_ITEM_VIEW_H_

#include <memory>

#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

class Browser;
class ExtensionContextMenuController;
class ExtensionsMenuButton;
class HoverButton;
class Profile;
class ToolbarActionViewController;
class ToolbarActionsModel;

// ExtensionsMenuItemView is a single row inside the extensions menu for a
// particular extension. Includes information about the extension in addition to
// a button to pin the extension to the toolbar and a button for accessing the
// associated context menu.
class ExtensionsMenuItemView : public views::View {
 public:
  METADATA_HEADER(ExtensionsMenuItemView);

  static constexpr int kMenuItemHeightDp = 40;
  static constexpr gfx::Size kIconSize{28, 28};

  ExtensionsMenuItemView(
      Browser* browser,
      std::unique_ptr<ToolbarActionViewController> controller,
      bool allow_pinning);
  ExtensionsMenuItemView(const ExtensionsMenuItemView&) = delete;
  ExtensionsMenuItemView& operator=(const ExtensionsMenuItemView&) = delete;
  ~ExtensionsMenuItemView() override;

  // views::View:
  void OnThemeChanged() override;

  void UpdatePinButton();

  bool IsContextMenuRunning() const;
  bool IsPinned() const;

  void ContextMenuPressed();
  void PinButtonPressed();

  ToolbarActionViewController* view_controller() { return controller_.get(); }
  const ToolbarActionViewController* view_controller() const {
    return controller_.get();
  }

  ExtensionsMenuButton* primary_action_button_for_testing();
  HoverButton* context_menu_button_for_testing() {
    return context_menu_button_;
  }
  HoverButton* pin_button_for_testing() { return pin_button_; }

 private:
  // Maybe adjust |icon_color| to assure high enough contrast with the
  // background.
  SkColor GetAdjustedIconColor(SkColor icon_color) const;

  Profile* const profile_;

  ExtensionsMenuButton* const primary_action_button_;

  std::unique_ptr<ToolbarActionViewController> controller_;

  HoverButton* context_menu_button_ = nullptr;

  ToolbarActionsModel* const model_;

  HoverButton* pin_button_ = nullptr;

  // This controller is responsible for showing the context menu for an
  // extension.
  std::unique_ptr<ExtensionContextMenuController> context_menu_controller_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_ITEM_VIEW_H_
