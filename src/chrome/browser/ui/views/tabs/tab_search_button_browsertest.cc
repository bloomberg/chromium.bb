// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_search_button.h"

#include <vector>

#include "base/feature_list.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/bubble/webui_bubble_dialog_view.h"
#include "chrome/browser/ui/views/bubble/webui_bubble_manager.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/tabs/tab_search_button.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/views/test/button_test_api.h"

namespace {
ui::MouseEvent GetDummyEvent() {
  return ui::MouseEvent(ui::ET_MOUSE_PRESSED, gfx::PointF(), gfx::PointF(),
                        base::TimeTicks::Now(), 0, 0);
}
}  // namespace

class TabSearchButtonBrowserTest : public InProcessBrowserTest {
 public:
#if defined(OS_WIN)
  // InProcessBrowserTest:
  void SetUp() override {
    scoped_feature_list_.InitAndDisableFeature(
        features::kWin10TabSearchCaptionButton);
    InProcessBrowserTest::SetUp();
  }
#endif  // defined(OS_WIN);

  BrowserView* browser_view() {
    return BrowserView::GetBrowserViewForBrowser(browser());
  }

  TabSearchButton* tab_search_button() {
    return browser_view()->tab_strip_region_view()->tab_search_button();
  }

  TabSearchBubbleHost* tab_search_bubble_host() {
    return tab_search_button()->tab_search_bubble_host();
  }

  WebUIBubbleManager* bubble_manager() {
    return tab_search_bubble_host()->webui_bubble_manager_for_testing();
  }

  void RunUntilBubbleWidgetDestroyed() {
    ASSERT_NE(nullptr, bubble_manager()->GetBubbleWidget());
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  run_loop.QuitClosure());
    run_loop.Run();
    ASSERT_EQ(nullptr, bubble_manager()->GetBubbleWidget());
  }

#if defined(OS_WIN)
 private:
  base::test::ScopedFeatureList scoped_feature_list_;
#endif  // defined(OS_WIN);
};

IN_PROC_BROWSER_TEST_F(TabSearchButtonBrowserTest, ButtonClickCreatesBubble) {
  ASSERT_EQ(nullptr, bubble_manager()->GetBubbleWidget());
  views::test::ButtonTestApi(tab_search_button()).NotifyClick(GetDummyEvent());
  ASSERT_NE(nullptr, bubble_manager()->GetBubbleWidget());

  tab_search_bubble_host()->CloseTabSearchBubble();
  ASSERT_TRUE(bubble_manager()->GetBubbleWidget()->IsClosed());

  RunUntilBubbleWidgetDestroyed();
}

class TabSearchButtonBrowserUITest : public DialogBrowserTest {
 public:
  // DialogBrowserTest:
#if defined(OS_WIN)
  void SetUp() override {
    scoped_feature_list_.InitAndDisableFeature(
        features::kWin10TabSearchCaptionButton);
    DialogBrowserTest::SetUp();
  }
#endif  // defined(OS_WIN);
  void ShowUi(const std::string& name) override {
    AppendTab(chrome::kChromeUISettingsURL);
    AppendTab(chrome::kChromeUIHistoryURL);
    AppendTab(chrome::kChromeUIBookmarksURL);
    auto* tab_search_button = BrowserView::GetBrowserViewForBrowser(browser())
                                  ->tab_strip_region_view()
                                  ->tab_search_button();
    views::test::ButtonTestApi(tab_search_button).NotifyClick(GetDummyEvent());
  }

  void AppendTab(std::string url) {
    chrome::AddTabAt(browser(), GURL(url), -1, true);
  }

#if defined(OS_WIN)
 private:
  base::test::ScopedFeatureList scoped_feature_list_;
#endif  // defined(OS_WIN);
};

// Invokes a tab search bubble.
IN_PROC_BROWSER_TEST_F(TabSearchButtonBrowserUITest, InvokeUi_default) {
  ShowAndVerifyUi();
}
