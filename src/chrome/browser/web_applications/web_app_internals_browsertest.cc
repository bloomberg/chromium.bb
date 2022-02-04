// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/callback_helpers.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/os_integration_manager.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {

namespace {

constexpr char kBadIconErrorTemplate[] = R"({
   "!url": "$1banners/manifest_test_page.html",
   "background_installation": false,
   "install_source": 15,
   "stages": [ {
      "!stage": "OnIconsRetrieved",
      "icons_downloaded_result": "Completed",
      "icons_http_results": [ {
         "http_code_desc": "Not Found",
         "http_status_code": 404,
         "icon_url": "$1banners/bad_icon.png"
      } ],
      "is_generated_icon": true
   } ]
}
)";

// Drops all CR and LF characters.
std::string TrimLineEndings(base::StringPiece text) {
  return CollapseWhitespaceASCII(text,
                                 /*trim_sequences_with_line_breaks=*/true);
}

}  // namespace

class WebAppInternalsBrowserTest : public InProcessBrowserTest {
 public:
  WebAppInternalsBrowserTest() = default;
  WebAppInternalsBrowserTest(const WebAppInternalsBrowserTest&) = delete;
  WebAppInternalsBrowserTest& operator=(const WebAppInternalsBrowserTest&) =
      delete;

  ~WebAppInternalsBrowserTest() override = default;

  void SetUp() override {
    embedded_test_server()->AddDefaultHandlers(GetChromeTestDataDir());
    embedded_test_server()->RegisterRequestHandler(
        base::BindRepeating(&WebAppInternalsBrowserTest::RequestHandlerOverride,
                            base::Unretained(this)));
    ASSERT_TRUE(embedded_test_server()->Start());

    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    web_app::test::WaitUntilReady(
        web_app::WebAppProvider::GetForTest(browser()->profile()));
    InProcessBrowserTest::SetUpOnMainThread();
  }

  AppId InstallWebApp(const GURL& app_url) {
    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), app_url));

    AppId app_id;
    base::RunLoop run_loop;
    GetProvider().install_manager().InstallWebAppFromManifestWithFallback(
        browser()->tab_strip_model()->GetActiveWebContents(),
        /*force_shortcut_app=*/false,
        webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON,
        base::BindOnce(test::TestAcceptDialogCallback),
        base::BindLambdaForTesting(
            [&](const AppId& new_app_id, InstallResultCode code) {
              EXPECT_EQ(code, InstallResultCode::kSuccessNewInstall);
              app_id = new_app_id;
              run_loop.Quit();
            }));
    run_loop.Run();
    return app_id;
  }

  WebAppProvider& GetProvider() {
    return *WebAppProvider::GetForTest(browser()->profile());
  }

  std::unique_ptr<net::test_server::HttpResponse> RequestHandlerOverride(
      const net::test_server::HttpRequest& request) {
    if (request_override_)
      return request_override_.Run(request);
    return nullptr;
  }

  void OverrideHttpRequest(GURL url, net::HttpStatusCode http_status_code) {
    request_override_ = base::BindLambdaForTesting(
        [url = std::move(url),
         http_status_code](const net::test_server::HttpRequest& request)
            -> std::unique_ptr<net::test_server::HttpResponse> {
          if (request.GetURL() != url)
            return nullptr;
          auto http_response =
              std::make_unique<net::test_server::BasicHttpResponse>();
          http_response->set_code(http_status_code);
          return std::move(http_response);
        });
  }

 private:
  net::EmbeddedTestServer::HandleRequestCallback request_override_;

  OsIntegrationManager::ScopedSuppressForTesting os_hooks_suppress_;

  base::test::ScopedFeatureList scoped_feature_list_{
      features::kRecordWebAppDebugInfo};
};

IN_PROC_BROWSER_TEST_F(WebAppInternalsBrowserTest,
                       PRE_InstallManagerErrorsPersist) {
  OverrideHttpRequest(embedded_test_server()->GetURL("/banners/bad_icon.png"),
                      net::HTTP_NOT_FOUND);

  AppId app_id = InstallWebApp(embedded_test_server()->GetURL(
      "/banners/manifest_test_page.html?manifest=manifest_bad_icon.json"));

  const WebApp* web_app = GetProvider().registrar().GetAppById(app_id);
  ASSERT_TRUE(web_app);
  EXPECT_TRUE(web_app->is_generated_icon());

  const std::string expected_error = base::ReplaceStringPlaceholders(
      kBadIconErrorTemplate, {embedded_test_server()->base_url().spec()},
      nullptr);

  ASSERT_TRUE(GetProvider().install_manager().error_log());
  ASSERT_EQ(1u, GetProvider().install_manager().error_log()->size());

  const base::Value& error_log =
      GetProvider().install_manager().error_log()->at(0);

  EXPECT_EQ(4u, error_log.DictSize());

  EXPECT_EQ(TrimLineEndings(expected_error),
            TrimLineEndings(error_log.DebugString()));
}

IN_PROC_BROWSER_TEST_F(WebAppInternalsBrowserTest,
                       InstallManagerErrorsPersist) {
  web_app::test::WaitUntilReady(
      web_app::WebAppProvider::GetForTest(browser()->profile()));

  ASSERT_TRUE(GetProvider().install_manager().error_log());
  ASSERT_EQ(1u, GetProvider().install_manager().error_log()->size());

  const base::Value& error_log =
      GetProvider().install_manager().error_log()->at(0);

  EXPECT_EQ(4u, error_log.DictSize());

  // Parses base url from the log: the port for embedded_test_server() changes
  // on every test run.
  const std::string* url_value = error_log.FindStringKey("!url");
  ASSERT_TRUE(url_value);
  GURL url{*url_value};
  ASSERT_TRUE(url.is_valid());

  const std::string expected_error = base::ReplaceStringPlaceholders(
      kBadIconErrorTemplate, {url.GetWithEmptyPath().spec()}, nullptr);

  EXPECT_EQ(TrimLineEndings(expected_error),
            TrimLineEndings(error_log.DebugString()));
}

}  // namespace web_app
