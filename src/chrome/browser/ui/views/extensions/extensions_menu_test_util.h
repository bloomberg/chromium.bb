// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_TEST_UTIL_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_TEST_UTIL_H_

#include <memory>
#include <string>

#include "base/auto_reset.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/extensions/extension_action_test_helper.h"

class Browser;
class ExtensionsMenuItemView;
class ExtensionsMenuView;
class ExtensionsToolbarContainer;

// An implementation of ExtensionActionTestHelper that works with the
// ExtensionsMenu.
class ExtensionsMenuTestUtil : public ExtensionActionTestHelper {
 public:
  ExtensionsMenuTestUtil(Browser* browser, bool is_real_window);
  ExtensionsMenuTestUtil(const ExtensionsMenuTestUtil&) = delete;
  ExtensionsMenuTestUtil& operator=(const ExtensionsMenuTestUtil&) = delete;
  ~ExtensionsMenuTestUtil() override;

  // ExtensionActionTestHelper:
  int NumberOfBrowserActions() override;
  int VisibleBrowserActions() override;
  bool HasAction(const extensions::ExtensionId& id) override;
  void InspectPopup(const extensions::ExtensionId& id) override;
  bool HasIcon(const extensions::ExtensionId& id) override;
  gfx::Image GetIcon(const extensions::ExtensionId& id) override;
  void Press(const extensions::ExtensionId& id) override;
  std::string GetTooltip(const extensions::ExtensionId& id) override;
  gfx::NativeView GetPopupNativeView() override;
  bool HasPopup() override;
  bool HidePopup() override;
  void SetWidth(int width) override;
  ExtensionsContainer* GetExtensionsContainer() override;
  void WaitForExtensionsContainerLayout() override;
  std::unique_ptr<ExtensionActionTestHelper> CreateOverflowBar(
      Browser* browser) override;
  void LayoutForOverflowBar() override;
  // TODO(devlin): Some of these popup methods have a common implementation
  // between this and ExtensionActionTestHelperViews. It would make sense to
  // extract them (since they aren't dependent on the extension action UI
  // implementation).
  gfx::Size GetMinPopupSize() override;
  gfx::Size GetMaxPopupSize() override;
  gfx::Size GetToolbarActionSize() override;
  gfx::Size GetMaxAvailableSizeToFitBubbleOnScreen(
      const extensions::ExtensionId& id) override;

 private:
  class MenuViewObserver;
  class Wrapper;

  // Returns the ExtensionsMenuItemView for the given `id` from the
  // `menu_view`.
  ExtensionsMenuItemView* GetMenuItemViewForId(
      const extensions::ExtensionId& id);

  // An override to allow test instances of the ExtensionsMenuView.
  // This has to be defined before |menu_view_| below.
  base::AutoReset<bool> scoped_allow_extensions_menu_instances_;

  std::unique_ptr<Wrapper> wrapper_;

  const raw_ptr<Browser> browser_;
  raw_ptr<ExtensionsToolbarContainer> extensions_container_ = nullptr;

  // Helps make sure that |menu_view_| set to null when destroyed by the widget
  // or via manual means.
  std::unique_ptr<MenuViewObserver> menu_view_observer_;

  // The owned version of |menu_view_|. Strongly prefer using |menu_view_|. May
  // be null when ownership is conditionally transferred to the bubble.
  std::unique_ptr<ExtensionsMenuView> owned_menu_view_;

  // The actual pointer to an ExtensionsMenuView, non-null if alive.
  ExtensionsMenuView* menu_view_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_TEST_UTIL_H_
