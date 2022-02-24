// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/sync/sync_encryption_keys_tab_helper.h"

#include <string>
#include <tuple>
#include <vector>

#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync/driver/sync_service_impl.h"
#include "components/sync/driver/trusted_vault_client.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/prerender_test_util.h"
#include "google_apis/gaia/gaia_switches.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

namespace {

const char kFakeGaiaId[] = "fake_gaia_id";
const char kConsoleMessage[] = "setSyncEncryptionKeys:Done";

// Executes JS to call chrome.setSyncEncryptionKeys().
void ExecJsSetSyncEncryptionKeys(content::RenderFrameHost* render_frame_host,
                                 const std::vector<uint8_t>& keys) {
  // To simplify the test, it limits the size of `keys` to 1.
  DCHECK_EQ(keys.size(), 1u);
  const std::string set_encryption_keys_script = base::StringPrintf(
      R"(
      let buffer = new ArrayBuffer(1);
      let view = new Uint8Array(buffer);
      view[0] = %d;
      chrome.setSyncEncryptionKeys(
          () => {console.log('%s');},
          "%s", [buffer], 0);
    )",
      keys[0], kConsoleMessage, kFakeGaiaId);

  std::ignore = content::ExecJs(render_frame_host, set_encryption_keys_script);
}

std::vector<std::vector<uint8_t>> FetchTrustedVaultKeysForProfile(
    Profile* profile,
    const AccountInfo& account_info) {
  syncer::SyncServiceImpl* sync_service =
      SyncServiceFactory::GetAsSyncServiceImplForProfile(profile);
  syncer::TrustedVaultClient* trusted_vault_client =
      sync_service->GetSyncClientForTest()->GetTrustedVaultClient();

  // Waits until the sync trusted vault keys have been received and stored.
  base::RunLoop loop;
  std::vector<std::vector<uint8_t>> actual_keys;

  trusted_vault_client->FetchKeys(
      account_info, base::BindLambdaForTesting(
                        [&](const std::vector<std::vector<uint8_t>>& keys) {
                          actual_keys = keys;
                          loop.Quit();
                        }));
  loop.Run();
  return actual_keys;
}

class SyncEncryptionKeysTabHelperBrowserTest : public InProcessBrowserTest {
 public:
  SyncEncryptionKeysTabHelperBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS),
        prerender_helper_(base::BindRepeating(
            &SyncEncryptionKeysTabHelperBrowserTest::web_contents,
            base::Unretained(this))) {}

  ~SyncEncryptionKeysTabHelperBrowserTest() override = default;

 protected:
  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  net::EmbeddedTestServer* https_server() { return &https_server_; }

  content::test::PrerenderTestHelper& prerender_helper() {
    return prerender_helper_;
  }

  content::test::FencedFrameTestHelper& fenced_frame_test_helper() {
    return fenced_frame_test_helper_;
  }

  bool HasEncryptionKeysApi(content::RenderFrameHost* rfh) {
    auto* tab_helper =
        SyncEncryptionKeysTabHelper::FromWebContents(web_contents());
    return tab_helper->HasEncryptionKeysApiForTesting(rfh);
  }

 private:
  void SetUp() override {
    ASSERT_TRUE(https_server_.InitializeAndListen());
    InProcessBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Override the sign-in URL so that it includes correct port from the test
    // server.
    command_line->AppendSwitchASCII(
        ::switches::kGaiaUrl,
        https_server()->GetURL("accounts.google.com", "/").spec());

    // Ignore cert errors so that the sign-in URL can be loaded from a site
    // other than localhost (the EmbeddedTestServer serves a certificate that
    // is valid for localhost).
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
    InProcessBrowserTest::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    https_server()->AddDefaultHandlers(GetChromeTestDataDir());
    https_server()->StartAcceptingConnections();
  }

  net::EmbeddedTestServer https_server_;
  content::test::FencedFrameTestHelper fenced_frame_test_helper_;
  content::test::PrerenderTestHelper prerender_helper_;
};

// Tests that chrome.setSyncEncryptionKeys() doesn't work in prerendering.
// If it is called in prerendering, it triggers canceling the prerendering
// and EncryptionKeyApi is not bound.
IN_PROC_BROWSER_TEST_F(SyncEncryptionKeysTabHelperBrowserTest,
                       ShouldNotBindEncryptionKeysApiInPrerendering) {
  base::HistogramTester histogram_tester;
  const GURL signin_url =
      https_server()->GetURL("accounts.google.com", "/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), signin_url));
  // EncryptionKeysApi is created for the primary page.
  EXPECT_TRUE(HasEncryptionKeysApi(web_contents()->GetMainFrame()));

  const GURL prerendering_url =
      https_server()->GetURL("accounts.google.com", "/simple.html");

  int host_id = prerender_helper().AddPrerender(prerendering_url);
  content::RenderFrameHostWrapper prerendered_frame_host(
      prerender_helper().GetPrerenderedMainFrameHost(host_id));

  // EncryptionKeysApi is also created for prerendering since it's a main frame
  // as well.
  EXPECT_TRUE(HasEncryptionKeysApi(prerendered_frame_host.get()));

  content::test::PrerenderHostObserver host_observer(*web_contents(), host_id);

  {
    content::WebContentsConsoleObserver console_observer(web_contents());
    console_observer.SetPattern(kConsoleMessage);

    // Calling setSyncEncryptionKeys() in the prerendered page triggers
    // canceling the prerendering since it's a associated interface and the
    // default policy is `MojoBinderAssociatedPolicy::kCancel`.
    const std::vector<uint8_t> kExpectedEncryptionKey = {7};
    ExecJsSetSyncEncryptionKeys(prerendered_frame_host.get(),
                                kExpectedEncryptionKey);
    host_observer.WaitForDestroyed();
    EXPECT_EQ(0u, console_observer.messages().size());
    histogram_tester.ExpectUniqueSample(
        "Prerender.Experimental.PrerenderCancelledInterface.SpeculationRule",
        4 /*PrerenderCancelledInterface::kSyncEncryptionKeysExtension*/, 1);
    EXPECT_TRUE(prerendered_frame_host.IsRenderFrameDeleted());
  }

  prerender_helper().NavigatePrimaryPage(prerendering_url);
  // Ensure that loading `prerendering_url` is not activated from prerendering.
  EXPECT_FALSE(host_observer.was_activated());
  auto* primary_main_frame = web_contents()->GetMainFrame();
  // Ensure that the main frame has EncryptionKeysApi.
  EXPECT_TRUE(HasEncryptionKeysApi(primary_main_frame));

  {
    content::WebContentsConsoleObserver console_observer(web_contents());
    console_observer.SetPattern(kConsoleMessage);

    // Calling setSyncEncryptionKeys() in the primary page works and it gets
    // the callback by setSyncEncryptionKeys().
    const std::vector<uint8_t> kExpectedEncryptionKey = {7};
    ExecJsSetSyncEncryptionKeys(primary_main_frame, kExpectedEncryptionKey);
    console_observer.Wait();
    EXPECT_EQ(1u, console_observer.messages().size());

    AccountInfo account;
    account.gaia = kFakeGaiaId;
    std::vector<std::vector<uint8_t>> actual_keys =
        FetchTrustedVaultKeysForProfile(browser()->profile(), account);
    EXPECT_THAT(actual_keys, testing::ElementsAre(kExpectedEncryptionKey));
  }
}

// Tests that chrome.setSyncEncryptionKeys() works in a fenced frame.
IN_PROC_BROWSER_TEST_F(SyncEncryptionKeysTabHelperBrowserTest,
                       ShouldBindEncryptionKeysApiInFencedFrame) {
  const GURL init_url =
      https_server()->GetURL("accounts.google.com", "/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), init_url));
  // EncryptionKeysApi is created for the primary page as the origin is allowed.
  ASSERT_TRUE(HasEncryptionKeysApi(web_contents()->GetMainFrame()));

  const GURL main_url = https_server()->GetURL("accounts.google.com",
                                               "/fenced_frames/title1.html");
  auto* fenced_frame_host = fenced_frame_test_helper().CreateFencedFrame(
      web_contents()->GetMainFrame(), main_url);
  // EncryptionKeysApi is also created for a fenced frame since it's a main
  // frame as well.
  EXPECT_TRUE(HasEncryptionKeysApi(fenced_frame_host));

  content::WebContentsConsoleObserver console_observer(web_contents());
  console_observer.SetPattern("setSyncEncryptionKeys:Done");

  // Calling setSyncEncryptionKeys() in the fenced frame works and it gets
  // the callback by setSyncEncryptionKeys().
  const std::vector<uint8_t> kExpectedEncryptionKey = {7};
  ExecJsSetSyncEncryptionKeys(fenced_frame_host, kExpectedEncryptionKey);
  console_observer.Wait();
  EXPECT_EQ(1u, console_observer.messages().size());

  AccountInfo account;
  account.gaia = kFakeGaiaId;
  std::vector<std::vector<uint8_t>> actual_keys =
      FetchTrustedVaultKeysForProfile(browser()->profile(), account);
  EXPECT_THAT(actual_keys, testing::ElementsAre(kExpectedEncryptionKey));
}

}  // namespace
