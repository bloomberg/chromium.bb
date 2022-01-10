// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_TOOLBAR_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_TOOLBAR_BUTTON_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/menu_button_controller.h"
#include "ui/views/widget/widget_observer.h"

class Browser;
class ExtensionsToolbarContainer;


// Button in the toolbar that provides access to the corresponding extensions
// menu.
class ExtensionsToolbarButton : public ToolbarButton,
                                public views::WidgetObserver {
 public:
  METADATA_HEADER(ExtensionsToolbarButton);

  enum class ButtonType {
    // Indicates that at least one extension is enabled, and opens the installed
    // extensions tab in the menu.
    kExtensions,
    // Indicates that at least one extension has access to the current page, and
    // opens the permissions tab in the menu.
    kSiteAccess
  };

  ExtensionsToolbarButton(Browser* browser,
                          ExtensionsToolbarContainer* extensions_container,
                          ButtonType button_type);
  ExtensionsToolbarButton(const ExtensionsToolbarButton&) = delete;
  ExtensionsToolbarButton& operator=(const ExtensionsToolbarButton&) = delete;
  ~ExtensionsToolbarButton() override;

  // Toggle the Extensions menu. If the ExtensionsToolbarContainer is in
  // kAutoHide mode and hidden this will cause it to show.
  void ToggleExtensionsMenu();

  bool GetExtensionsMenuShowing() const;

  // ToolbarButton:
  gfx::Size CalculatePreferredSize() const override;
  gfx::Size GetMinimumSize() const override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  void UpdateIcon() override;

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

 private:
  int GetIconSize() const;

  // A lock to keep the button pressed when a popup is visible.
  std::unique_ptr<views::MenuButtonController::PressedLock> pressed_lock_;

  const raw_ptr<Browser> browser_;
  const ButtonType button_type_;
  raw_ptr<views::MenuButtonController> menu_button_controller_;
  const raw_ptr<ExtensionsToolbarContainer> extensions_container_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_TOOLBAR_BUTTON_H_
