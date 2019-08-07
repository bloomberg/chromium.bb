// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_menu_view.h"

#include <algorithm>

#include "base/path_service.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_button.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_button.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/webui_url_constants.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/test/widget_test.h"

class ExtensionsMenuViewBrowserTest : public DialogBrowserTest {
 protected:
  void LoadTestExtension(const std::string& extension) {
    extensions::ChromeTestExtensionLoader loader(browser()->profile());
    base::FilePath test_data_dir;
    base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
    extensions_.push_back(
        loader.LoadExtension(test_data_dir.AppendASCII(extension)));
  }

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kExtensionsToolbarMenu);
    DialogBrowserTest::SetUp();
  }

  void ShowUi(const std::string& name) override {
    ui::MouseEvent click_event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                               base::TimeTicks(), ui::EF_LEFT_MOUSE_BUTTON, 0);
    BrowserView::GetBrowserViewForBrowser(browser())
        ->toolbar()
        ->GetExtensionsButton()
        ->OnMousePressed(click_event);
  }

  static std::vector<ExtensionsMenuButton*> GetExtensionMenuButtons() {
    std::vector<ExtensionsMenuButton*> buttons;
    for (auto* view : ExtensionsMenuView::GetExtensionsMenuViewForTesting()
                          ->extension_menu_button_container_for_testing()
                          ->children()) {
      if (view->GetClassName() == ExtensionsMenuButton::kClassName)
        buttons.push_back(static_cast<ExtensionsMenuButton*>(view));
    }
    return buttons;
  }

  std::vector<ToolbarActionView*> GetToolbarActionViews() const {
    std::vector<ToolbarActionView*> views;
    for (auto* view : BrowserView::GetBrowserViewForBrowser(browser())
                          ->toolbar()
                          ->extensions_container()
                          ->children()) {
      if (view->GetClassName() == ToolbarActionView::kClassName)
        views.push_back(static_cast<ToolbarActionView*>(view));
    }
    return views;
  }

  std::vector<ToolbarActionView*> GetVisibleToolbarActionViews() const {
    auto views = GetToolbarActionViews();
    base::EraseIf(views, [](views::View* view) { return !view->GetVisible(); });
    return views;
  }

  void TriggerSingleExtensionButton() {
    auto buttons = GetExtensionMenuButtons();
    ASSERT_EQ(1u, buttons.size());
    ui::MouseEvent click_event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                               base::TimeTicks(), ui::EF_LEFT_MOUSE_BUTTON, 0);
    views::test::ButtonTestApi test_api(buttons[0]);
    test_api.NotifyClick(click_event);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  std::vector<scoped_refptr<const extensions::Extension>> extensions_;
};

IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewBrowserTest, InvokeUi_default) {
  LoadTestExtension("extensions/uitest/long_name");
  LoadTestExtension("extensions/uitest/window_open");

  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewBrowserTest, InvokeUi_NoExtensions) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewBrowserTest, TriggerPopup) {
  LoadTestExtension("extensions/simple_with_popup");
  ShowUi("");
  VerifyUi();

  ExtensionsContainer* const extensions_container =
      BrowserView::GetBrowserViewForBrowser(browser())
          ->toolbar()
          ->extensions_container();

  EXPECT_EQ(nullptr, extensions_container->GetPoppedOutAction());
  EXPECT_TRUE(GetVisibleToolbarActionViews().empty());

  TriggerSingleExtensionButton();

  // After triggering an extension with a popup, there should a popped-out
  // action and show the view.
  auto visible_icons = GetVisibleToolbarActionViews();
  EXPECT_NE(nullptr, extensions_container->GetPoppedOutAction());
  ASSERT_EQ(1u, visible_icons.size());
  EXPECT_EQ(extensions_container->GetPoppedOutAction(),
            visible_icons[0]->view_controller());

  extensions_container->HideActivePopup();

  // After dismissing the popup there should no longer be a popped-out action
  // and the icon should no longer be visible in the extensions container.
  EXPECT_EQ(nullptr, extensions_container->GetPoppedOutAction());
  EXPECT_TRUE(GetVisibleToolbarActionViews().empty());
}

IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewBrowserTest,
                       TriggeringExtensionClosesMenu) {
  LoadTestExtension("extensions/trigger_actions/browser_action");
  ShowUi("");
  VerifyUi();

  EXPECT_TRUE(ExtensionsMenuView::IsShowing());

  views::test::WidgetDestroyedWaiter destroyed_waiter(
      ExtensionsMenuView::GetExtensionsMenuViewForTesting()->GetWidget());
  TriggerSingleExtensionButton();

  destroyed_waiter.Wait();

  ExtensionsContainer* const extensions_container =
      BrowserView::GetBrowserViewForBrowser(browser())
          ->toolbar()
          ->extensions_container();

  // This test should not use a popped-out action, as we want to make sure that
  // the menu closes on its own and not because a popup dialog replaces it.
  EXPECT_EQ(nullptr, extensions_container->GetPoppedOutAction());

  EXPECT_FALSE(ExtensionsMenuView::IsShowing());
}

IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewBrowserTest,
                       CreatesOneButtonPerExtension) {
  LoadTestExtension("extensions/uitest/long_name");
  LoadTestExtension("extensions/uitest/window_open");
  ShowUi("");
  VerifyUi();
  EXPECT_EQ(2u, extensions_.size());
  EXPECT_EQ(extensions_.size(), GetExtensionMenuButtons().size());
  DismissUi();
}

IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewBrowserTest,
                       ManageExtensionsOpensExtensionsPage) {
  ShowUi("");
  VerifyUi();

  EXPECT_TRUE(ExtensionsMenuView::IsShowing());

  ui::MouseEvent click_event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                             base::TimeTicks(), ui::EF_LEFT_MOUSE_BUTTON, 0);
  views::test::ButtonTestApi test_api(
      ExtensionsMenuView::GetExtensionsMenuViewForTesting()
          ->manage_extensions_button_for_testing());
  test_api.NotifyClick(click_event);

  // Clicking the Manage Extensions button should open chrome://extensions.
  EXPECT_EQ(
      chrome::kChromeUIExtensionsURL,
      browser()->tab_strip_model()->GetActiveWebContents()->GetVisibleURL());
}

// TODO(pbos): Add test coverage that makes sure removing popped-out extensions
// properly disposes of the popup.
