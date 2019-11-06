// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/macros.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/search/instant_test_base.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

class ThirdPartyNTPBrowserTest : public InProcessBrowserTest,
                                 public InstantTestBase {
 public:
  ThirdPartyNTPBrowserTest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(https_test_server().Start());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ThirdPartyNTPBrowserTest);
};

// Verifies that a third party NTP can successfully embed the most visited
// iframe.
IN_PROC_BROWSER_TEST_F(ThirdPartyNTPBrowserTest, EmbeddedMostVisitedIframe) {
  GURL base_url =
      https_test_server().GetURL("ntp.com", "/instant_extended.html");
  GURL ntp_url =
      https_test_server().GetURL("ntp.com", "/instant_extended_ntp.html");
  InstantTestBase::Init(base_url, ntp_url, false);

  SetupInstant(browser());

  // Navigate to the NTP URL and verify that the resulting process is marked as
  // an Instant process.
  ui_test_utils::NavigateToURL(browser(), ntp_url);
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  InstantService* instant_service =
      InstantServiceFactory::GetForProfile(browser()->profile());
  EXPECT_TRUE(instant_service->IsInstantProcess(
      contents->GetMainFrame()->GetProcess()->GetID()));

  // Add a chrome-search://most-visited/title.html?rid=1&fs=0 subframe and
  // verify that navigation completes successfully, with no kills.
  content::TestNavigationObserver nav_observer(contents);
  const char* kScript = R"(
      const frame = document.createElement('iframe');
      frame.src = 'chrome-search://most-visited/title.html?rid=1&fs=0';
      document.body.appendChild(frame);
  )";
  ASSERT_TRUE(content::ExecJs(contents, kScript));
  nav_observer.WaitForNavigationFinished();

  // Verify that the subframe exists and has the expected origin.
  ASSERT_EQ(2u, contents->GetAllFrames().size());
  content::RenderFrameHost* subframe = contents->GetAllFrames()[1];
  std::string subframe_origin;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      subframe, "domAutomationController.send(window.origin)",
      &subframe_origin));
  EXPECT_EQ("chrome-search://most-visited", subframe_origin);
}
