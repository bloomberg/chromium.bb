// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_hover_card_bubble_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/ui_test_utils.h"
#include "ui/views/controls/label.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

using views::Widget;

// Helper to wait until a window is deactivated.
class WindowDeactivedWaiter : public views::WidgetObserver {
 public:
  explicit WindowDeactivedWaiter(BrowserView* window) : window_(window) {
    window_->frame()->AddObserver(this);
  }
  ~WindowDeactivedWaiter() override { window_->frame()->RemoveObserver(this); }

  void Wait() {
    if (!window_->IsActive())
      return;
    run_loop_.Run();
  }

  // WidgetObserver overrides:
  void OnWidgetActivationChanged(Widget* widget, bool active) override {
    if (!active)
      run_loop_.Quit();
  }

 private:
  BrowserView* const window_;
  base::RunLoop run_loop_;
};

// Helper to wait until the hover card widget is visible.
class HoverCardVisibleWaiter : public views::WidgetObserver {
 public:
  explicit HoverCardVisibleWaiter(Widget* hover_card)
      : hover_card_(hover_card) {
    hover_card_->AddObserver(this);
  }
  ~HoverCardVisibleWaiter() override { hover_card_->RemoveObserver(this); }

  void Wait() {
    if (hover_card_->IsVisible())
      return;
    run_loop_.Run();
  }

  // WidgetObserver overrides:
  void OnWidgetVisibilityChanged(Widget* widget, bool visible) override {
    if (visible)
      run_loop_.Quit();
  }

 private:
  Widget* const hover_card_;
  base::RunLoop run_loop_;
};

class TabHoverCardBubbleViewBrowserTest : public DialogBrowserTest {
 public:
  TabHoverCardBubbleViewBrowserTest() = default;
  ~TabHoverCardBubbleViewBrowserTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kTabHoverCards);
    DialogBrowserTest::SetUp();
  }

  static TabHoverCardBubbleView* GetHoverCard(const TabStrip* tabstrip) {
    return tabstrip->hover_card_;
  }

  static Widget* GetHoverCardWidget(const TabHoverCardBubbleView* hover_card) {
    return hover_card->widget_;
  }

  const base::string16& GetHoverCardTitle(
      const TabHoverCardBubbleView* hover_card) {
    return hover_card->title_label_->text();
  }

  const base::string16& GetHoverCardDomain(
      const TabHoverCardBubbleView* hover_card) {
    return hover_card->domain_label_->text();
  }

  void MouseExitTabStrip() {
    TabStrip* tab_strip =
        BrowserView::GetBrowserViewForBrowser(browser())->tabstrip();
    ui::MouseEvent stop_hover_event(ui::ET_MOUSE_EXITED, gfx::Point(),
                                    gfx::Point(), base::TimeTicks(),
                                    ui::EF_NONE, 0);
    tab_strip->OnMouseExited(stop_hover_event);
  }

  void ClickMouseOnTab() {
    TabStrip* tab_strip =
        BrowserView::GetBrowserViewForBrowser(browser())->tabstrip();
    Tab* tab = tab_strip->tab_at(0);
    ui::MouseEvent click_event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                               base::TimeTicks(), ui::EF_NONE, 0);
    tab->OnMousePressed(click_event);
  }

  void HoverMouseOverTabAt(int index) {
    TabStrip* tab_strip =
        BrowserView::GetBrowserViewForBrowser(browser())->tabstrip();
    Tab* tab = tab_strip->tab_at(index);
    ui::MouseEvent hover_event(ui::ET_MOUSE_ENTERED, gfx::Point(), gfx::Point(),
                               base::TimeTicks(), ui::EF_NONE, 0);
    tab->OnMouseEntered(hover_event);
  }

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    TabStrip* tab_strip =
        BrowserView::GetBrowserViewForBrowser(browser())->tabstrip();
    Tab* tab = tab_strip->tab_at(0);
    ui::MouseEvent hover_event(ui::ET_MOUSE_ENTERED, gfx::Point(), gfx::Point(),
                               base::TimeTicks(), ui::EF_NONE, 0);
    tab->OnMouseEntered(hover_event);
    TabHoverCardBubbleView* hover_card = GetHoverCard(tab_strip);
    Widget* widget = GetHoverCardWidget(hover_card);
    HoverCardVisibleWaiter waiter(widget);
    waiter.Wait();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TabHoverCardBubbleViewBrowserTest);

  base::test::ScopedFeatureList scoped_feature_list_;
};

// Fails on win7 (dbg): http://crbug.com/932402.
#if defined(OS_WIN) && !defined(NDEBUG)
#define MAYBE_InvokeUi_tab_hover_card DISABLED_InvokeUi_tab_hover_card
#else
#define MAYBE_InvokeUi_tab_hover_card InvokeUi_tab_hover_card
#endif
IN_PROC_BROWSER_TEST_F(TabHoverCardBubbleViewBrowserTest,
                       MAYBE_InvokeUi_tab_hover_card) {
  ShowAndVerifyUi();
}

// Verify hover card is visible while hovering and not visible outside of the
// tabstrip.
IN_PROC_BROWSER_TEST_F(TabHoverCardBubbleViewBrowserTest,
                       WidgetVisibleOnHover) {
  ShowUi("default");
  TabStrip* tab_strip =
      BrowserView::GetBrowserViewForBrowser(browser())->tabstrip();
  TabHoverCardBubbleView* hover_card = GetHoverCard(tab_strip);
  Widget* widget = GetHoverCardWidget(hover_card);

  EXPECT_TRUE(widget != nullptr);
  EXPECT_TRUE(widget->IsVisible());
  MouseExitTabStrip();
  EXPECT_FALSE(widget->IsVisible());
}

// Verify hover card is not visible after clicking on a tab.
IN_PROC_BROWSER_TEST_F(TabHoverCardBubbleViewBrowserTest,
                       WidgetNotVisibleOnClick) {
  ShowUi("default");
  TabStrip* tab_strip =
      BrowserView::GetBrowserViewForBrowser(browser())->tabstrip();
  TabHoverCardBubbleView* hover_card = GetHoverCard(tab_strip);
  Widget* widget = GetHoverCardWidget(hover_card);

  EXPECT_TRUE(widget != nullptr);
  EXPECT_TRUE(widget->IsVisible());
  ClickMouseOnTab();
  EXPECT_FALSE(widget->IsVisible());
}

// Verify title, domain, and anchor are correctly updated when moving hover
// from one tab to another.
IN_PROC_BROWSER_TEST_F(TabHoverCardBubbleViewBrowserTest, WidgetDataUpdate) {
  TabStrip* tab_strip =
      BrowserView::GetBrowserViewForBrowser(browser())->tabstrip();
  TabRendererData newTabData = TabRendererData();
  newTabData.title = base::UTF8ToUTF16("Test Tab 2");
  newTabData.url = GURL("http://example.com/this/should/not/be/seen");
  tab_strip->AddTabAt(1, newTabData, false);

  ShowUi("default");
  TabHoverCardBubbleView* hover_card = GetHoverCard(tab_strip);
  Widget* widget = GetHoverCardWidget(hover_card);

  EXPECT_TRUE(widget != nullptr);
  EXPECT_TRUE(widget->IsVisible());
  HoverMouseOverTabAt(1);
  EXPECT_TRUE(widget != nullptr);
  EXPECT_TRUE(widget->IsVisible());
  Tab* tab = tab_strip->tab_at(1);
  EXPECT_EQ(GetHoverCardTitle(hover_card), base::UTF8ToUTF16("Test Tab 2"));
  EXPECT_EQ(GetHoverCardDomain(hover_card), base::UTF8ToUTF16("example.com"));
  EXPECT_EQ(hover_card->GetAnchorView(), static_cast<views::View*>(tab));
}

// Verify inactive window remains inactive when showing a hover card for a tab
// in the inactive window.
IN_PROC_BROWSER_TEST_F(TabHoverCardBubbleViewBrowserTest,
                       InactiveWindowStaysInactiveOnHover) {
  const BrowserList* active_browser_list_ = BrowserList::GetInstance();
  // Open a second window. On Windows, Widget::Deactivate() works by activating
  // the next topmost window on the z-order stack. So instead we use two windows
  // and Widget::Activate() here to prevent Widget::Deactivate() from
  // undesirably activating another window when tests are being run in parallel.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(url::kAboutBlankURL), WindowOpenDisposition::NEW_WINDOW,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_BROWSER);
  ShowUi("default");
  ASSERT_EQ(2u, active_browser_list_->size());
  Browser* active_window = active_browser_list_->get(0);
  Browser* inactive_window = active_browser_list_->get(1);
  WindowDeactivedWaiter waiter(
      BrowserView::GetBrowserViewForBrowser(inactive_window));
  BrowserView::GetBrowserViewForBrowser(active_window)->Activate();
  waiter.Wait();
  ASSERT_FALSE(
      BrowserView::GetBrowserViewForBrowser(inactive_window)->IsActive());
  TabStrip* tab_strip =
      BrowserView::GetBrowserViewForBrowser(inactive_window)->tabstrip();
  Tab* tab = tab_strip->tab_at(0);
  ui::MouseEvent hover_event(ui::ET_MOUSE_ENTERED, gfx::Point(), gfx::Point(),
                             base::TimeTicks(), ui::EF_NONE, 0);
  tab->OnMouseEntered(hover_event);
  ASSERT_FALSE(
      BrowserView::GetBrowserViewForBrowser(inactive_window)->IsActive());
}
