// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/toolbar_view.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "content/public/test/browser_test.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

using bookmarks::BookmarkModel;

class ToolbarViewTest : public InProcessBrowserTest {
 public:
  ToolbarViewTest() = default;
  ToolbarViewTest(const ToolbarViewTest&) = delete;
  ToolbarViewTest& operator=(const ToolbarViewTest&) = delete;

  void RunToolbarCycleFocusTest(Browser* browser);
};

void ToolbarViewTest::RunToolbarCycleFocusTest(Browser* browser) {
  gfx::NativeWindow window = browser->window()->GetNativeWindow();
  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window);

  // Test relies on browser window activation, while platform such as Linux's
  // window activation is asynchronous.
  views::test::WidgetActivationWaiter waiter(widget, true);
  waiter.Wait();

  // Send focus to the toolbar as if the user pressed Alt+Shift+T. This should
  // happen after the browser window activation.
  CommandUpdater* updater = browser->command_controller();
  updater->ExecuteCommand(IDC_FOCUS_TOOLBAR);

  views::FocusManager* focus_manager = widget->GetFocusManager();
  views::View* first_view = focus_manager->GetFocusedView();
  std::vector<int> ids;

  // Press Tab to cycle through all of the controls in the toolbar until
  // we end up back where we started.
  bool found_reload = false;
  bool found_location_bar = false;
  bool found_app_menu = false;
  const views::View* view = NULL;
  while (view != first_view) {
    focus_manager->AdvanceFocus(false);
    view = focus_manager->GetFocusedView();
    ids.push_back(view->GetID());
    if (view->GetID() == VIEW_ID_RELOAD_BUTTON)
      found_reload = true;
    if (view->GetID() == VIEW_ID_APP_MENU)
      found_app_menu = true;
    if (view->GetID() == VIEW_ID_OMNIBOX)
      found_location_bar = true;
    if (ids.size() > 100)
      GTEST_FAIL() << "Tabbed 100 times, still haven't cycled back!";
  }

  // Make sure we found a few key items.
  ASSERT_TRUE(found_reload);
  ASSERT_TRUE(found_app_menu);
  ASSERT_TRUE(found_location_bar);

  // Now press Shift-Tab to cycle backwards.
  std::vector<int> reverse_ids;
  view = NULL;
  while (view != first_view) {
    focus_manager->AdvanceFocus(true);
    view = focus_manager->GetFocusedView();
    reverse_ids.push_back(view->GetID());
    if (reverse_ids.size() > 100)
      GTEST_FAIL() << "Tabbed 100 times, still haven't cycled back!";
  }

  // Assert that the views were focused in exactly the reverse order.
  // The sequences should be the same length, and the last element will
  // be the same, and the others are reverse.
  ASSERT_EQ(ids.size(), reverse_ids.size());
  size_t count = ids.size();
  for (size_t i = 0; i < count - 1; i++)
    EXPECT_EQ(ids[i], reverse_ids[count - 2 - i]);
  EXPECT_EQ(ids[count - 1], reverse_ids[count - 1]);
}

IN_PROC_BROWSER_TEST_F(ToolbarViewTest, ToolbarCycleFocus) {
  RunToolbarCycleFocusTest(browser());
}

IN_PROC_BROWSER_TEST_F(ToolbarViewTest, ToolbarCycleFocusWithBookmarkBar) {
  CommandUpdater* updater = browser()->command_controller();
  updater->ExecuteCommand(IDC_SHOW_BOOKMARK_BAR);

  BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(browser()->profile());
  bookmarks::AddIfNotBookmarked(model, GURL("http://foo.com"), u"Foo");

  // We want to specifically test the case where the bookmark bar is
  // already showing when a window opens, so create a second browser
  // window with the same profile.
  Browser* second_browser = CreateBrowser(browser()->profile());
  RunToolbarCycleFocusTest(second_browser);
}

IN_PROC_BROWSER_TEST_F(ToolbarViewTest, BackButtonUpdate) {
  ToolbarButtonProvider* toolbar_button_provider =
      BrowserView::GetBrowserViewForBrowser(browser())->toolbar();
  EXPECT_FALSE(toolbar_button_provider->GetBackButton()->GetEnabled());

  // Navigate to title1.html. Back button should be enabled.
  GURL url = ui_test_utils::GetTestUrl(
      base::FilePath(), base::FilePath(FILE_PATH_LITERAL("title1.html")));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  EXPECT_TRUE(toolbar_button_provider->GetBackButton()->GetEnabled());

  // Delete old navigations. Back button will be disabled.
  auto& controller =
      browser()->tab_strip_model()->GetActiveWebContents()->GetController();
  controller.DeleteNavigationEntries(base::BindRepeating(
      [&](content::NavigationEntry* entry) { return true; }));
  EXPECT_FALSE(toolbar_button_provider->GetBackButton()->GetEnabled());
}

IN_PROC_BROWSER_TEST_F(ToolbarViewTest,
                       ToolbarForRegularProfileHasExtensionsToolbarContainer) {
  // Verify the normal browser has an extensions toolbar container.
  ExtensionsToolbarContainer* extensions_container =
      BrowserView::GetBrowserViewForBrowser(browser())
          ->toolbar()
          ->extensions_container();
  EXPECT_NE(nullptr, extensions_container);
}

// TODO(crbug.com/991596): Setup test profiles properly for CrOS.
#if BUILDFLAG(IS_CHROMEOS_ASH)
#define MAYBE_ExtensionsToolbarContainerForGuest \
  DISABLED_ExtensionsToolbarContainerForGuest
#else
#define MAYBE_ExtensionsToolbarContainerForGuest \
  ExtensionsToolbarContainerForGuest
#endif
IN_PROC_BROWSER_TEST_F(ToolbarViewTest,
                       MAYBE_ExtensionsToolbarContainerForGuest) {
  // Verify guest browser does not have an extensions toolbar container.
  profiles::SwitchToGuestProfile(ProfileManager::CreateCallback());
  ui_test_utils::WaitForBrowserToOpen();
  Profile* guest = g_browser_process->profile_manager()->GetProfileByPath(
      ProfileManager::GetGuestProfilePath());
  ASSERT_TRUE(guest);
  Browser* target_browser = chrome::FindAnyBrowser(guest, true);
  ASSERT_TRUE(target_browser);
  ExtensionsToolbarContainer* extensions_container =
      BrowserView::GetBrowserViewForBrowser(target_browser)
          ->toolbar()
          ->extensions_container();
  EXPECT_EQ(nullptr, extensions_container);
}
