// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/page_info/android/page_info_client.h"
#include "components/page_info/page_info_delegate.h"
#include "weblayer/browser/tab_impl.h"
#include "weblayer/browser/url_bar/page_info_delegate_impl.h"
#include "weblayer/shell/browser/shell.h"
#include "weblayer/test/weblayer_browser_test.h"
#include "weblayer/test/weblayer_browser_test_utils.h"

namespace weblayer {

class PageInfoBrowserTest : public WebLayerBrowserTest {
 protected:
  content::WebContents* GetWebContents() {
    Tab* tab = shell()->tab();
    TabImpl* tab_impl = static_cast<TabImpl*>(tab);
    return tab_impl->web_contents();
  }
};

IN_PROC_BROWSER_TEST_F(PageInfoBrowserTest, PageInfoClientSet) {
  EXPECT_TRUE(page_info::GetPageInfoClient());
}

IN_PROC_BROWSER_TEST_F(PageInfoBrowserTest, ContentNotDisplayedInVrHeadset) {
  std::unique_ptr<PageInfoDelegate> page_info_delegate =
      page_info::GetPageInfoClient()->CreatePageInfoDelegate(GetWebContents());
  ASSERT_TRUE(page_info_delegate);
  EXPECT_FALSE(page_info_delegate->IsContentDisplayedInVrHeadset());
}

IN_PROC_BROWSER_TEST_F(PageInfoBrowserTest, StatefulSSLHostStateDelegateSet) {
  std::unique_ptr<PageInfoDelegate> page_info_delegate =
      page_info::GetPageInfoClient()->CreatePageInfoDelegate(GetWebContents());
  ASSERT_TRUE(page_info_delegate);
  EXPECT_TRUE(page_info_delegate->GetStatefulSSLHostStateDelegate());
}

IN_PROC_BROWSER_TEST_F(PageInfoBrowserTest, PermissionDecisionAutoblocker) {
  std::unique_ptr<PageInfoDelegate> page_info_delegate =
      page_info::GetPageInfoClient()->CreatePageInfoDelegate(GetWebContents());
  ASSERT_TRUE(page_info_delegate);
  EXPECT_TRUE(page_info_delegate->GetPermissionDecisionAutoblocker());
}

IN_PROC_BROWSER_TEST_F(PageInfoBrowserTest, ContentSettings) {
  std::unique_ptr<PageInfoDelegate> page_info_delegate =
      page_info::GetPageInfoClient()->CreatePageInfoDelegate(GetWebContents());
  ASSERT_TRUE(page_info_delegate);
  EXPECT_TRUE(page_info_delegate->GetContentSettings());
}

IN_PROC_BROWSER_TEST_F(PageInfoBrowserTest, PermissionStatus) {
  std::unique_ptr<PageInfoDelegate> page_info_delegate =
      page_info::GetPageInfoClient()->CreatePageInfoDelegate(GetWebContents());
  ASSERT_TRUE(page_info_delegate);
  GURL url("https://example.com");

  auto* content_settings_map = page_info_delegate->GetContentSettings();
  ASSERT_TRUE(content_settings_map);
  content_settings_map->SetContentSettingDefaultScope(
      url, url, ContentSettingsType::BACKGROUND_SYNC, std::string(),
      CONTENT_SETTING_BLOCK);

  // Check that |page_info_delegate| returns expected ContentSettingsType.
  EXPECT_EQ(page_info_delegate
                ->GetPermissionStatus(ContentSettingsType::NOTIFICATIONS, url)
                .content_setting,
            CONTENT_SETTING_BLOCK);
}

IN_PROC_BROWSER_TEST_F(PageInfoBrowserTest,
                       TabSpecificContentSettingsDelegate) {
  std::unique_ptr<PageInfoDelegate> page_info_delegate =
      page_info::GetPageInfoClient()->CreatePageInfoDelegate(GetWebContents());
  ASSERT_TRUE(page_info_delegate);
  EXPECT_TRUE(page_info_delegate->GetTabSpecificContentSettingsDelegate());
}

IN_PROC_BROWSER_TEST_F(PageInfoBrowserTest, EmbedderNameSet) {
  std::unique_ptr<PageInfoDelegate> page_info_delegate =
      page_info::GetPageInfoClient()->CreatePageInfoDelegate(GetWebContents());
  ASSERT_TRUE(page_info_delegate);
  base::string16 expected_embedder_name =
      base::ASCIIToUTF16("WebLayerBrowserTests");
  EXPECT_EQ(expected_embedder_name,
            page_info_delegate->GetClientApplicationName().c_str());
}

}  // namespace weblayer
