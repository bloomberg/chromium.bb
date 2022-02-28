// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tab_search_bubble_host.h"

#include "base/run_loop.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/bubble/webui_bubble_manager.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/base/accelerators/accelerator.h"

class TabSearchBubbleHostBrowserTest : public InProcessBrowserTest {
 public:
  BrowserView* browser_view() {
    return BrowserView::GetBrowserViewForBrowser(browser());
  }

  TabSearchBubbleHost* tab_search_bubble_host() {
    return browser_view()->GetTabSearchBubbleHost();
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
};

IN_PROC_BROWSER_TEST_F(TabSearchBubbleHostBrowserTest,
                       BubbleShowTimerTriggersCorrectly) {
  ASSERT_EQ(nullptr, bubble_manager()->GetBubbleWidget());
  tab_search_bubble_host()->ShowTabSearchBubble();

  // |bubble_created_time_| should be set as soon as the bubble widget is
  // created.
  EXPECT_FALSE(bubble_manager()->GetBubbleWidget()->IsVisible());
  EXPECT_TRUE(tab_search_bubble_host()->bubble_created_time_for_testing());

  // Showing the bubble should reset the timestamp.
  bubble_manager()->bubble_view_for_testing()->ShowUI();
  EXPECT_TRUE(bubble_manager()->GetBubbleWidget()->IsVisible());
  EXPECT_FALSE(tab_search_bubble_host()->bubble_created_time_for_testing());

  tab_search_bubble_host()->CloseTabSearchBubble();
  RunUntilBubbleWidgetDestroyed();
}

// On macOS, most accelerators are handled by CommandDispatcher.
#if !defined(OS_MAC)
IN_PROC_BROWSER_TEST_F(TabSearchBubbleHostBrowserTest,
                       KeyboardShortcutTriggersBubble) {
  ASSERT_EQ(nullptr, bubble_manager()->GetBubbleWidget());

  auto accelerator = ui::Accelerator(
      ui::VKEY_A, ui::EF_SHIFT_DOWN | ui::EF_PLATFORM_ACCELERATOR);
  browser_view()->AcceleratorPressed(accelerator);

  // Accelerator keys should have created the tab search bubble.
  ASSERT_NE(nullptr, bubble_manager()->GetBubbleWidget());

  tab_search_bubble_host()->CloseTabSearchBubble();
  ASSERT_TRUE(bubble_manager()->GetBubbleWidget()->IsClosed());

  RunUntilBubbleWidgetDestroyed();
}
#endif
