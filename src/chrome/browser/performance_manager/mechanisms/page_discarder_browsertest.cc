// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/mechanisms/page_discarder.h"

#include "base/run_loop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/performance_manager/public/performance_manager.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "url/gurl.h"

namespace performance_manager {

using PageDiscarderBrowserTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(PageDiscarderBrowserTest, DiscardPageNodes) {
  ASSERT_TRUE(embedded_test_server()->Start());

  content::WindowedNotificationObserver load(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::NotificationService::AllSources());
  content::OpenURLParams page(
      embedded_test_server()->GetURL("a.com", "/foo.html"), content::Referrer(),
      WindowOpenDisposition::NEW_BACKGROUND_TAB, ui::PAGE_TRANSITION_TYPED,
      false);
  auto* contents = browser()->OpenURL(page);
  load.Wait();

  base::RunLoop run_loop;
  auto quit_closure = run_loop.QuitClosure();
  PerformanceManager::CallOnGraph(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<PageNode> page_node,
             base::OnceClosure quit_closure) {
            EXPECT_TRUE(page_node);
            mechanism::PageDiscarder discarder;
            discarder.DiscardPageNodes(
                {page_node.get()},
                base::BindOnce(
                    [](base::OnceClosure quit_closure, bool success) {
                      EXPECT_TRUE(success);
                      std::move(quit_closure).Run();
                    },
                    std::move(quit_closure)));
          },
          PerformanceManager::GetPrimaryPageNodeForWebContents(contents),
          std::move(quit_closure)));
  run_loop.Run();

  auto* new_contents = browser()->tab_strip_model()->GetWebContentsAt(1);
  EXPECT_TRUE(new_contents->WasDiscarded());
}

}  // namespace performance_manager
