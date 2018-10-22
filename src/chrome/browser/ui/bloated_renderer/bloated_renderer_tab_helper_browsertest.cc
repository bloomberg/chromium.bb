// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bloated_renderer/bloated_renderer_tab_helper.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"

using BloatedRendererTabHelperBrowserTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(BloatedRendererTabHelperBrowserTest, ReloadBloatedTab) {
  content::WindowedNotificationObserver load(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::NotificationService::AllSources());
  content::OpenURLParams url(
      GURL(chrome::kChromeUIAboutURL), content::Referrer(),
      WindowOpenDisposition::CURRENT_TAB, ui::PAGE_TRANSITION_TYPED, false);
  browser()->OpenURL(url);
  load.Wait();

  content::WindowedNotificationObserver reload(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::NotificationService::AllSources());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents);
  EXPECT_EQ(0u, infobar_service->infobar_count());

  auto* page_signal_receiver =
      resource_coordinator::PageSignalReceiver::GetInstance();
  resource_coordinator::PageNavigationIdentity page_id;
  page_id.navigation_id =
      page_signal_receiver->GetNavigationIDForWebContents(web_contents);
  BloatedRendererTabHelper::FromWebContents(web_contents)
      ->OnRendererIsBloated(web_contents, page_id);
  reload.Wait();
  EXPECT_EQ(1u, infobar_service->infobar_count());
}
