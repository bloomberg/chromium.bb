// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/extension_action_test_util.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_action.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/switches.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace extensions {
namespace {

const std::string kFeedPage = "/feeds/feed.html";
const std::string kNoFeedPage = "/feeds/no_feed.html";

const std::string kHashPageA =
    "/extensions/api_test/page_action/hash_change/test_page_A.html";
const std::string kHashPageAHash = kHashPageA + "#asdf";
const std::string kHashPageB =
    "/extensions/api_test/page_action/hash_change/test_page_B.html";

IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, PageActionCrash25562) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // This page action will not show an icon, since it doesn't specify one but
  // is included here to test for a crash (http://crbug.com/25562).
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("browsertest")
                    .AppendASCII("crash_25562")));

  // Navigate to the feed page.
  GURL feed_url = embedded_test_server()->GetURL(kFeedPage);
  ui_test_utils::NavigateToURL(browser(), feed_url);
  // We should now have one page action ready to go in the LocationBar.
  ASSERT_TRUE(WaitForPageActionVisibilityChangeTo(1));
}

// Tests that we can load page actions in the Omnibox.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, PageAction) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("subscribe_page_action")));

  ASSERT_TRUE(WaitForPageActionVisibilityChangeTo(0));

  // Navigate to the feed page.
  GURL feed_url = embedded_test_server()->GetURL(kFeedPage);
  ui_test_utils::NavigateToURL(browser(), feed_url);
  // We should now have one page action ready to go in the LocationBar.
  ASSERT_TRUE(WaitForPageActionVisibilityChangeTo(1));

  // Navigate to a page with no feed.
  GURL no_feed = embedded_test_server()->GetURL(kNoFeedPage);
  ui_test_utils::NavigateToURL(browser(), no_feed);
  // Make sure the page action goes away.
  ASSERT_TRUE(WaitForPageActionVisibilityChangeTo(0));
}

// Tests that we don't lose the page action icon on same-document navigations.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, PageActionSameDocumentNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());

  base::FilePath extension_path(test_data_dir_.AppendASCII("api_test")
                                        .AppendASCII("page_action")
                                        .AppendASCII("hash_change"));
  ASSERT_TRUE(LoadExtension(extension_path));

  // Page action should become visible when we navigate here.
  GURL feed_url = embedded_test_server()->GetURL(kHashPageA);
  ui_test_utils::NavigateToURL(browser(), feed_url);
  ASSERT_TRUE(WaitForPageActionVisibilityChangeTo(1));

  // Same-document navigation, page action should remain.
  feed_url = embedded_test_server()->GetURL(kHashPageAHash);
  ui_test_utils::NavigateToURL(browser(), feed_url);
  ASSERT_TRUE(WaitForPageActionVisibilityChangeTo(1));

  // Not a same-document navigation, page action should go away.
  feed_url = embedded_test_server()->GetURL(kHashPageB);
  ui_test_utils::NavigateToURL(browser(), feed_url);
  ASSERT_TRUE(WaitForPageActionVisibilityChangeTo(0));
}

// Tests that the location bar forgets about unloaded page actions.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, UnloadPageAction) {
  ASSERT_TRUE(embedded_test_server()->Start());

  base::FilePath extension_path(
      test_data_dir_.AppendASCII("subscribe_page_action"));
  ASSERT_TRUE(LoadExtension(extension_path));

  // Navigation prompts the location bar to load page actions.
  GURL feed_url = embedded_test_server()->GetURL(kFeedPage);
  ui_test_utils::NavigateToURL(browser(), feed_url);
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(1u, extension_action_test_util::GetTotalPageActionCount(tab));

  UnloadExtension(last_loaded_extension_id());

  // Make sure the page action goes away when it's unloaded.
  EXPECT_EQ(0u, extension_action_test_util::GetTotalPageActionCount(tab));
}

// Tests that we can load page actions in the Omnibox.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, PageActionRefreshCrash) {
  base::TimeTicks start_time = base::TimeTicks::Now();

  ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(browser()->profile());

  size_t size_before = registry->enabled_extensions().size();

  base::FilePath base_path = test_data_dir_.AppendASCII("browsertest")
                                     .AppendASCII("crash_44415");
  // Load extension A.
  const Extension* extensionA = LoadExtension(base_path.AppendASCII("ExtA"));
  ASSERT_TRUE(extensionA);
  ASSERT_TRUE(WaitForPageActionVisibilityChangeTo(1));
  ASSERT_EQ(size_before + 1, registry->enabled_extensions().size());

  LOG(INFO) << "Load extension A done  : "
            << (base::TimeTicks::Now() - start_time).InMilliseconds()
            << " ms" << std::flush;

  // Load extension B.
  const Extension* extensionB = LoadExtension(base_path.AppendASCII("ExtB"));
  ASSERT_TRUE(extensionB);
  ASSERT_TRUE(WaitForPageActionVisibilityChangeTo(2));
  ASSERT_EQ(size_before + 2, registry->enabled_extensions().size());

  LOG(INFO) << "Load extension B done  : "
            << (base::TimeTicks::Now() - start_time).InMilliseconds()
            << " ms" << std::flush;

  std::string idA = extensionA->id();
  ReloadExtension(extensionA->id());
  // ExtensionA has changed, so refetch it.
  ASSERT_EQ(size_before + 2, registry->enabled_extensions().size());
  extensionA = registry->enabled_extensions().GetByID(idA);

  LOG(INFO) << "Reload extension A done: "
            << (base::TimeTicks::Now() - start_time).InMilliseconds()
            << " ms" << std::flush;

  ReloadExtension(extensionB->id());

  LOG(INFO) << "Reload extension B done: "
            << (base::TimeTicks::Now() - start_time).InMilliseconds()
            << " ms" << std::flush;

  // This is where it would crash, before http://crbug.com/44415 was fixed.
  ReloadExtension(extensionA->id());

  LOG(INFO) << "Test completed         : "
            << (base::TimeTicks::Now() - start_time).InMilliseconds()
            << " ms" << std::flush;
}

}  // namespace
}  // namespace extensions
