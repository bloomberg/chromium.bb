// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/values.h"
#include "chrome/browser/extensions/api/tabs/tabs_api.h"
#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/common/extension_builder.h"

namespace extensions {

namespace keys = tabs_constants;
namespace utils = extension_function_test_utils;

using ExtensionTabsTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(ExtensionTabsTest, GetLastFocusedWindow) {
  // Create a new window which making it the "last focused" window.
  // Note that "last focused" means the "top" most window.
  Browser* new_browser = CreateBrowser(browser()->profile());
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(new_browser));

  GURL url("about:blank");
  AddTabAtIndexToBrowser(new_browser, 0, url, ui::PAGE_TRANSITION_LINK, true);

  int focused_window_id =
      extensions::ExtensionTabUtil::GetWindowId(new_browser);

  scoped_refptr<extensions::WindowsGetLastFocusedFunction> function =
      new extensions::WindowsGetLastFocusedFunction();
  scoped_refptr<extensions::Extension> extension(
      extensions::ExtensionBuilder("Test").Build());
  function->set_extension(extension.get());
  std::unique_ptr<base::DictionaryValue> result(
      utils::ToDictionary(utils::RunFunctionAndReturnSingleResult(
          function.get(), "[]", new_browser)));

  // The id should always match the last focused window and does not depend
  // on what was passed to RunFunctionAndReturnSingleResult.
  EXPECT_EQ(focused_window_id, api_test_utils::GetInteger(result.get(), "id"));
  base::ListValue* tabs = NULL;
  EXPECT_FALSE(result.get()->GetList(keys::kTabsKey, &tabs));

  function = new extensions::WindowsGetLastFocusedFunction();
  function->set_extension(extension.get());
  result.reset(utils::ToDictionary(
      utils::RunFunctionAndReturnSingleResult(function.get(),
                                              "[{\"populate\": true}]",
                                              browser())));

  // The id should always match the last focused window and does not depend
  // on what was passed to RunFunctionAndReturnSingleResult.
  EXPECT_EQ(focused_window_id, api_test_utils::GetInteger(result.get(), "id"));
  // "populate" was enabled so tabs should be populated.
  EXPECT_TRUE(result.get()->GetList(keys::kTabsKey, &tabs));
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsTest, QueryLastFocusedWindowTabs) {
  const size_t kExtraWindows = 2;
  for (size_t i = 0; i < kExtraWindows; ++i)
    CreateBrowser(browser()->profile());

  Browser* focused_window = CreateBrowser(browser()->profile());
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(focused_window));

  GURL url("about:blank");
  AddTabAtIndexToBrowser(focused_window, 0, url, ui::PAGE_TRANSITION_LINK,
                         true);
  int focused_window_id =
      extensions::ExtensionTabUtil::GetWindowId(focused_window);

  // Get tabs in the 'last focused' window called from non-focused browser.
  scoped_refptr<extensions::TabsQueryFunction> function =
      new extensions::TabsQueryFunction();
  scoped_refptr<extensions::Extension> extension(
      extensions::ExtensionBuilder("Test").Build());
  function->set_extension(extension.get());
  std::unique_ptr<base::ListValue> result(
      utils::ToList(utils::RunFunctionAndReturnSingleResult(
          function.get(), "[{\"lastFocusedWindow\":true}]", browser())));

  base::ListValue* result_tabs = result.get();
  // We should have one initial tab and one added tab.
  EXPECT_EQ(2u, result_tabs->GetSize());
  for (size_t i = 0; i < result_tabs->GetSize(); ++i) {
    base::DictionaryValue* result_tab = NULL;
    EXPECT_TRUE(result_tabs->GetDictionary(i, &result_tab));
    EXPECT_EQ(focused_window_id,
              api_test_utils::GetInteger(result_tab, keys::kWindowIdKey));
  }

  // Get tabs NOT in the 'last focused' window called from the focused browser.
  function = new extensions::TabsQueryFunction();
  function->set_extension(extension.get());
  result.reset(utils::ToList(
      utils::RunFunctionAndReturnSingleResult(function.get(),
                                              "[{\"lastFocusedWindow\":false}]",
                                              browser())));

  result_tabs = result.get();
  // We should get one tab for each extra window and one for the initial window.
  EXPECT_EQ(kExtraWindows + 1, result_tabs->GetSize());
  for (size_t i = 0; i < result_tabs->GetSize(); ++i) {
    base::DictionaryValue* result_tab = NULL;
    EXPECT_TRUE(result_tabs->GetDictionary(i, &result_tab));
    EXPECT_NE(focused_window_id,
              api_test_utils::GetInteger(result_tab, keys::kWindowIdKey));
  }
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, TabCurrentWindow) {
  ASSERT_TRUE(RunExtensionTest("tabs/current_window")) << message_;
}

}  // namespace extensions
