// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/browser_app_launcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/web_applications/test/test_server_redirect_handle.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/os_integration/web_app_file_handler_manager.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_prefs_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "components/embedder_support/switches.h"
#include "components/permissions/test/permission_request_observer.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_launcher.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/url_loader_interceptor.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/features.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/file_manager/file_manager_test_util.h"
#endif

namespace web_app {

class WebAppFileHandlingTestBase : public WebAppControllerBrowserTest {
 public:
  WebAppProvider* provider() { return WebAppProvider::GetForTest(profile()); }

  WebAppFileHandlerManager& file_handler_manager() {
    return provider()
        ->os_integration_manager()
        .file_handler_manager_for_testing();
  }

  WebAppRegistrar& registrar() { return provider()->registrar(); }

  GURL GetSecureAppURL() {
    return https_server()->GetURL("app.com", "/ssl/google.html");
  }

  GURL GetTextFileHandlerActionURL() {
    return https_server()->GetURL("app.com", "/ssl/blank_page.html");
  }

  GURL GetCSVFileHandlerActionURL() {
    return https_server()->GetURL("app.com", "/ssl/page_with_refs.html");
  }

  GURL GetHTMLFileHandlerActionURL() {
    return https_server()->GetURL("app.com", "/ssl/page_with_frame.html");
  }

  void InstallFileHandlingPWA() {
    GURL url = GetSecureAppURL();

    auto web_app_info = std::make_unique<WebAppInstallInfo>();
    web_app_info->start_url = url;
    web_app_info->scope = url.GetWithoutFilename();
    web_app_info->title = u"A Hosted App";

    // Basic plain text format.
    apps::FileHandler entry1;
    entry1.action = GetTextFileHandlerActionURL();
    entry1.accept.emplace_back();
    entry1.accept[0].mime_type = "text/*";
    entry1.accept[0].file_extensions.insert(".txt");
    web_app_info->file_handlers.push_back(std::move(entry1));

    // A format that the browser is also a handler for, to confirm that the
    // browser doesn't override PWAs using File Handling for types that the
    // browser also handles.
    apps::FileHandler entry2;
    entry2.action = GetHTMLFileHandlerActionURL();
    entry2.accept.emplace_back();
    entry2.accept[0].mime_type = "text/html";
    entry2.accept[0].file_extensions.insert(".html");
    web_app_info->file_handlers.push_back(std::move(entry2));

    // application/* format.
    apps::FileHandler entry3;
    entry3.action = GetCSVFileHandlerActionURL();
    entry3.accept.emplace_back();
    entry3.accept[0].mime_type = "application/csv";
    entry3.accept[0].file_extensions.insert(".csv");
    web_app_info->file_handlers.push_back(std::move(entry3));

    app_id_ =
        WebAppControllerBrowserTest::InstallWebApp(std::move(web_app_info));
  }

  AppId InstallAnotherFileHandlingPwa(const GURL& start_url) {
    auto web_app_info = std::make_unique<WebAppInstallInfo>();
    web_app_info->start_url = start_url;
    web_app_info->scope = start_url.GetWithoutFilename();
    web_app_info->title = u"A second app";

    // This one handles jpegs.
    apps::FileHandler entry1;
    entry1.action = GetTextFileHandlerActionURL();
    entry1.accept.emplace_back();
    entry1.accept[0].mime_type = "image/jpeg";
    entry1.accept[0].file_extensions.insert(".jpeg");
    web_app_info->file_handlers.push_back(std::move(entry1));

    return WebAppControllerBrowserTest::InstallWebApp(std::move(web_app_info));
  }

 protected:
  const AppId& app_id() { return app_id_; }

 private:
  AppId app_id_;
};

namespace {

base::FilePath NewTestFilePath(const base::StringPiece extension) {
  // CreateTemporaryFile blocks, temporarily allow blocking.
  base::ScopedAllowBlockingForTesting allow_blocking;

  // In order to test file handling, we need to be able to supply a file
  // extension for the temp file.
  base::FilePath test_file_path;
  base::CreateTemporaryFile(&test_file_path);
  base::FilePath new_file_path = test_file_path.AddExtensionASCII(extension);
  EXPECT_TRUE(base::ReplaceFile(test_file_path, new_file_path, nullptr));
  return new_file_path;
}

// Attach the launchParams to the window so we can inspect them easily.
void AttachTestConsumer(content::WebContents* web_contents) {
  auto result = content::EvalJs(web_contents,
                                "launchQueue.setConsumer(launchParams => {"
                                "  window.launchParams = launchParams;"
                                "});");
}

// Launches the |app_id| web app with |files| handles, awaits for
// |expected_launch_url| to load and stashes any launch params on
// "window.launchParams" for further inspection.
content::WebContents* LaunchApplication(
    Profile* profile,
    const std::string& app_id,
    const GURL& expected_launch_url,
    const apps::mojom::LaunchContainer launch_container =
        apps::mojom::LaunchContainer::kLaunchContainerWindow,
    const apps::mojom::LaunchSource launch_source =
        apps::mojom::LaunchSource::kFromTest,
    const std::vector<base::FilePath>& files = std::vector<base::FilePath>()) {
  apps::AppLaunchParams params(app_id, launch_container,
                               WindowOpenDisposition::NEW_WINDOW,
                               launch_source);

  if (files.size())
    params.launch_files = files;

  content::TestNavigationObserver navigation_observer(expected_launch_url);
  navigation_observer.StartWatchingNewWebContents();
  content::WebContents* web_contents =
      apps::AppServiceProxyFactory::GetForProfile(profile)
          ->BrowserAppLauncher()
          ->LaunchAppWithParamsForTesting(std::move(params));

  navigation_observer.Wait();
  AttachTestConsumer(web_contents);
  return web_contents;
}

}  // namespace

class WebAppFileHandlingBrowserTest : public WebAppFileHandlingTestBase {
 public:
  WebAppFileHandlingBrowserTest() : redirect_handle_(*https_server()) {}

  void LaunchWithFiles(
      const std::string& app_id,
      const GURL& expected_launch_url,
      const std::vector<base::FilePath>& files,
      const apps::mojom::LaunchContainer launch_container =
          apps::mojom::LaunchContainer::kLaunchContainerWindow) {
    web_contents_ = LaunchApplication(
        profile(), app_id, expected_launch_url, launch_container,
        apps::mojom::LaunchSource::kFromFileManager, files);
    destroyed_watcher_ =
        std::make_unique<content::WebContentsDestroyedWatcher>(web_contents_);
  }

  void VerifyPwaDidReceiveFileLaunchParams(
      bool expect_got_launch_params,
      const base::FilePath& expected_file_path = {}) {
    bool got_launch_params =
        content::EvalJs(web_contents_.get(), "!!window.launchParams")
            .ExtractBool();
    ASSERT_EQ(expect_got_launch_params, got_launch_params);
    if (got_launch_params) {
      EXPECT_EQ(1, content::EvalJs(web_contents_.get(),
                                   "window.launchParams.files.length"));
      EXPECT_EQ(expected_file_path.BaseName().AsUTF8Unsafe(),
                content::EvalJs(web_contents_.get(),
                                "window.launchParams.files[0].name"));
      std::string check_permissions_js(
          // clang-format off
        "(async () => {"
        "  return await window.launchParams.files[0].queryPermission("
        "             {mode: 'readwrite'}) === 'granted';"
        "})()");
      // clang-format on
      EXPECT_TRUE(content::EvalJs(web_contents_.get(), check_permissions_js)
                      .ExtractBool());
    }
  }

  void UninstallWebApp(const AppId& app_id) {
    base::RunLoop run_loop;
    UninstallWebAppWithCallback(
        profile(), app_id, base::BindLambdaForTesting([&](bool uninstalled) {
          EXPECT_TRUE(uninstalled);
          run_loop.Quit();
        }));
    run_loop.Run();
  }

  AppId InstallFileHandlingWebApp(const std::u16string& title,
                                  const GURL& handler_url) {
    auto web_app_info = std::make_unique<WebAppInstallInfo>();
    web_app_info->start_url =
        https_server()->GetURL("app.com", "/web_app_file_handling/index.html");
    web_app_info->scope = web_app_info->start_url.GetWithoutFilename();
    web_app_info->title = title;
    apps::FileHandler entry;
    entry.action = handler_url;
    entry.accept.emplace_back();
    entry.accept[0].mime_type = "text/*";
    entry.accept[0].file_extensions.insert(".txt");
    web_app_info->file_handlers.push_back(std::move(entry));
    return WebAppControllerBrowserTest::InstallWebApp(std::move(web_app_info));
  }

 protected:
  TestServerRedirectHandle redirect_handle_;
  base::test::ScopedFeatureList feature_list_{
      blink::features::kFileHandlingAPI};
  raw_ptr<content::WebContents> web_contents_;
  std::unique_ptr<content::WebContentsDestroyedWatcher> destroyed_watcher_;
};

IN_PROC_BROWSER_TEST_F(WebAppFileHandlingBrowserTest, ManifestFields) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL app_url(
      embedded_test_server()->GetURL("/web_app_file_handling/basic_app.html"));
  const AppId app_id = InstallWebAppFromManifest(browser(), app_url);
  const WebApp* web_app = registrar().GetAppById(app_id);
  ASSERT_TRUE(web_app);

  ASSERT_EQ(1U, web_app->file_handlers().size());
  EXPECT_EQ(embedded_test_server()->GetURL(
                "/web_app_file_handling/icons_app_load.html"),
            web_app->file_handlers()[0].action);
  EXPECT_EQ(u"Plain Text!", web_app->file_handlers()[0].display_name);
}

IN_PROC_BROWSER_TEST_F(WebAppFileHandlingBrowserTest,
                       LaunchConsumerIsNotTriggeredWithNoFiles) {
  InstallFileHandlingPWA();
  // The URL used is the normal start URL.
  LaunchWithFiles(app_id(), GetSecureAppURL(), {});
  VerifyPwaDidReceiveFileLaunchParams(false);
}

IN_PROC_BROWSER_TEST_F(WebAppFileHandlingBrowserTest,
                       PWAsCanReceiveFileLaunchParams) {
  InstallFileHandlingPWA();
  base::FilePath test_file_path = NewTestFilePath("txt");
  LaunchWithFiles(app_id(), GetTextFileHandlerActionURL(), {test_file_path});

  VerifyPwaDidReceiveFileLaunchParams(true, test_file_path);
}

IN_PROC_BROWSER_TEST_F(WebAppFileHandlingBrowserTest,
                       PWAsCanReceiveFileLaunchParamsInTab) {
  InstallFileHandlingPWA();
  base::FilePath test_file_path = NewTestFilePath("txt");
  LaunchWithFiles(app_id(), GetTextFileHandlerActionURL(), {test_file_path},
                  apps::mojom::LaunchContainer::kLaunchContainerTab);

  VerifyPwaDidReceiveFileLaunchParams(true, test_file_path);
}

IN_PROC_BROWSER_TEST_F(WebAppFileHandlingBrowserTest,
                       PWAsDispatchOnCorrectFileHandlingURL) {
  InstallFileHandlingPWA();

  // Test that file handler dispatches correct URL based on file extension.
  LaunchWithFiles(app_id(), GetSecureAppURL(), {});
  LaunchWithFiles(app_id(), GetTextFileHandlerActionURL(),
                  {NewTestFilePath("txt")});
  LaunchWithFiles(app_id(), GetHTMLFileHandlerActionURL(),
                  {NewTestFilePath("html")});
  LaunchWithFiles(app_id(), GetCSVFileHandlerActionURL(),
                  {NewTestFilePath("csv")});

  // Test as above in a tab.
  LaunchWithFiles(app_id(), GetSecureAppURL(), {},
                  apps::mojom::LaunchContainer::kLaunchContainerTab);
  LaunchWithFiles(app_id(), GetTextFileHandlerActionURL(),
                  {NewTestFilePath("txt")},
                  apps::mojom::LaunchContainer::kLaunchContainerTab);
  LaunchWithFiles(app_id(), GetHTMLFileHandlerActionURL(),
                  {NewTestFilePath("html")},
                  apps::mojom::LaunchContainer::kLaunchContainerTab);
  LaunchWithFiles(app_id(), GetCSVFileHandlerActionURL(),
                  {NewTestFilePath("csv")},
                  apps::mojom::LaunchContainer::kLaunchContainerTab);
}

// Regression test for crbug.com/1205528
IN_PROC_BROWSER_TEST_F(WebAppFileHandlingBrowserTest,
                       LaunchParamsEmptyIfFileUnhandled) {
  InstallFileHandlingPWA();

  // Test that file handler dispatches to the normal start URL when the file
  // path is not a handled file type, and `launchParams` remains undefined.
  LaunchWithFiles(app_id(), GetSecureAppURL(), {NewTestFilePath("png")});
  VerifyPwaDidReceiveFileLaunchParams(false);
}

// Regression test for crbug.com/1126091
IN_PROC_BROWSER_TEST_F(WebAppFileHandlingBrowserTest,
                       LaunchQueueSetOnRedirect) {
  GURL handler_url = https_server()->GetURL(
      "app.com", "/web_app_file_handling/handle_files_with_redirect.html");
  AppId app_id =
      InstallFileHandlingWebApp(u"An app that will be reloaded", handler_url);

  base::FilePath file = NewTestFilePath("txt");

  {
    auto redirect_scope = redirect_handle_.Redirect({
        .redirect_url = handler_url,
        .target_url = https_server()->GetURL(
            "app.com", "/web_app_file_handling/handle_files.html"),
        .origin = "app.com",
    });

    LaunchWithFiles(app_id, redirect_handle_.params().target_url, {file});
  }

  // The redirected-to page should get the launch queue.
  VerifyPwaDidReceiveFileLaunchParams(true, file);
}

IN_PROC_BROWSER_TEST_F(WebAppFileHandlingBrowserTest, LaunchQueueSetOnReload) {
  GURL handler_url = https_server()->GetURL(
      "app.com", "/web_app_file_handling/handle_files.html");
  AppId app_id =
      InstallFileHandlingWebApp(u"An app that will be reloaded", handler_url);

  base::FilePath file = NewTestFilePath("txt");
  LaunchWithFiles(app_id, handler_url, {file});
  VerifyPwaDidReceiveFileLaunchParams(true, file);

  // Reload the page.
  {
    content::TestNavigationObserver navigation_observer(web_contents_);
    chrome::Reload(chrome::FindBrowserWithWebContents(web_contents_),
                   WindowOpenDisposition::CURRENT_TAB);
    navigation_observer.Wait();
    AttachTestConsumer(web_contents_);
  }
  VerifyPwaDidReceiveFileLaunchParams(true, file);
}

IN_PROC_BROWSER_TEST_F(WebAppFileHandlingBrowserTest,
                       LaunchQueueSetOnReloadAfterPushState) {
  GURL handler_url = https_server()->GetURL(
      "app.com", "/web_app_file_handling/handle_files.html");
  AppId app_id =
      InstallFileHandlingWebApp(u"An app that will be reloaded", handler_url);

  base::FilePath file = NewTestFilePath("txt");
  LaunchWithFiles(app_id, handler_url, {file});
  VerifyPwaDidReceiveFileLaunchParams(true, file);

  // page initiates pushstate
  {
    content::TestNavigationObserver navigation_observer(web_contents_);
    auto result = content::EvalJs(web_contents_.get(),
                                  "window.history.replaceState(null, '', "
                                  "window.location.href + '#foo');");
    EXPECT_TRUE(result.error.empty());
    navigation_observer.Wait();
  }

  // Reload the page.
  {
    content::TestNavigationObserver navigation_observer(web_contents_);
    chrome::Reload(chrome::FindBrowserWithWebContents(web_contents_),
                   WindowOpenDisposition::CURRENT_TAB);
    navigation_observer.Wait();
    AttachTestConsumer(web_contents_);
  }
  VerifyPwaDidReceiveFileLaunchParams(true, file);
}

IN_PROC_BROWSER_TEST_F(WebAppFileHandlingBrowserTest,
                       LaunchQueueNotSetOnCrossOriginRedirect) {
  // Install an app where the file handling action page redirects to a page on a
  // different origin.
  GURL handler_url = https_server()->GetURL(
      "app.com",
      "/web_app_file_handling/handle_files_with_redirect_to_other_origin.html");
  AppId app_id =
      InstallFileHandlingWebApp(u"An app that will be reloaded", handler_url);
  base::FilePath file = NewTestFilePath("txt");

  {
    auto redirect_scope = redirect_handle_.Redirect({
        .redirect_url = handler_url,
        .target_url = https_server()->GetURL(
            "example.com", "/web_app_file_handling/handle_files.html"),
        .origin = "app.com",
    });

    LaunchWithFiles(app_id, redirect_handle_.params().target_url, {file});
  }

  // The redirected-to page should NOT get the launch queue.
  VerifyPwaDidReceiveFileLaunchParams(false);
}

IN_PROC_BROWSER_TEST_F(WebAppFileHandlingBrowserTest,
                       LaunchQueueNotSetOnNavigate) {
  GURL handler_url = https_server()->GetURL(
      "app.com", "/web_app_file_handling/handle_files.html");
  GURL start_url =
      https_server()->GetURL("app.com", "/web_app_file_handling/index.html");
  AppId app_id =
      InstallFileHandlingWebApp(u"An app that will be navigated", handler_url);

  base::FilePath file = NewTestFilePath("txt");
  LaunchWithFiles(app_id, handler_url, {file});
  VerifyPwaDidReceiveFileLaunchParams(true, file);

  // Navigating the page should not enqueue the LaunchParams again.
  ASSERT_TRUE(NavigateToURL(web_contents_, start_url));
  AttachTestConsumer(web_contents_);
  VerifyPwaDidReceiveFileLaunchParams(false);

  // Nor should navigating back to the handler page re-enqueue.
  ASSERT_TRUE(NavigateToURL(web_contents_, handler_url));
  AttachTestConsumer(web_contents_);
  VerifyPwaDidReceiveFileLaunchParams(false);
}

IN_PROC_BROWSER_TEST_F(WebAppFileHandlingBrowserTest,
                       SometimesResetPermission) {
  // Install the first app and simulate the user granting it the file handling
  // permission.
  InstallFileHandlingPWA();
  const GURL origin = GetSecureAppURL().DeprecatedGetOriginAsURL();

  EXPECT_EQ(ApiApprovalState::kRequiresPrompt,
            registrar().GetAppFileHandlerApprovalState(app_id()));
  provider()->sync_bridge().SetAppFileHandlerApprovalState(
      app_id(), ApiApprovalState::kAllowed);

  // Tangentially: make sure the outparam for
  // `GetFileTypeAssociationsHandledByWebAppsForDisplay` is properly set.
  bool plural = false;
  GetFileTypeAssociationsHandledByWebAppForDisplay(profile(), app_id(),
                                                   &plural);
  EXPECT_TRUE(plural);

  // Installing a different app should have no impact.
  GURL second_app_url = https_server()->GetURL("app.com", "/pwa/app2.html");
  InstallAnotherFileHandlingPwa(second_app_url);
  EXPECT_EQ(ApiApprovalState::kAllowed,
            registrar().GetAppById(app_id())->file_handler_approval_state());
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
// End-to-end test to ensure the file handler is registered on ChromeOS when the
// extension system is initialized. Gives more coverage than the unit tests.
IN_PROC_BROWSER_TEST_F(WebAppFileHandlingBrowserTest, IsFileHandlerOnChromeOS) {
  InstallFileHandlingPWA();

  base::FilePath test_file_path = NewTestFilePath("txt");
  std::vector<file_manager::file_tasks::FullTaskDescriptor> tasks =
      file_manager::test::GetTasksForFile(profile(), test_file_path);
  // Note that there are normally multiple tasks due to default-installed
  // handlers (e.g. add to zip file). But those handlers are not installed by
  // default in browser tests.
  ASSERT_EQ(1u, tasks.size());
  EXPECT_EQ(tasks[0].task_descriptor.app_id, app_id());
}

// Ensures correct behavior for files on "special volumes", such as file systems
// provided by extensions. These do not have local files (i.e. backed by
// inodes).
IN_PROC_BROWSER_TEST_F(WebAppFileHandlingBrowserTest,
                       NotHandlerForNonNativeFiles) {
  InstallFileHandlingPWA();
  base::WeakPtr<file_manager::Volume> fsp_volume =
      file_manager::test::InstallFileSystemProviderChromeApp(profile());

  // File in chrome/test/data/extensions/api_test/file_browser/image_provider/.
  base::FilePath test_file_path =
      fsp_volume->mount_path().AppendASCII("readonly.txt");
  std::vector<file_manager::file_tasks::FullTaskDescriptor> tasks =
      file_manager::test::GetTasksForFile(profile(), test_file_path);

  // Current expectation is for the task not to be found while the native
  // filesystem API is still being built up. See https://crbug.com/1079065.
  // When the "special file" check in file_manager::file_tasks::FindWebTasks()
  // is removed, this test should work the same as IsFileHandlerOnChromeOS.
  EXPECT_EQ(0u, tasks.size());
}

class WebAppFileHandlingDisabledTest : public WebAppFileHandlingBrowserTest {
 public:
  WebAppFileHandlingDisabledTest() : WebAppFileHandlingBrowserTest() {
    feature_list_.InitWithFeatures({}, {blink::features::kFileHandlingAPI});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Check that the web app is not returned as a file handler task when
// the flag kFileHandlingAPI is disabled.
IN_PROC_BROWSER_TEST_F(WebAppFileHandlingDisabledTest,
                       NoFileHandlerOnChromeOS) {
  InstallFileHandlingPWA();

  base::FilePath test_file_path = NewTestFilePath("txt");
  std::vector<file_manager::file_tasks::FullTaskDescriptor> tasks =
      file_manager::test::GetTasksForFile(profile(), test_file_path);
  EXPECT_EQ(0u, tasks.size());
}
#endif

class WebAppFileHandlingIconBrowserTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  WebAppFileHandlingIconBrowserTest() {
    feature_list_.InitWithFeatures({blink::features::kFileHandlingAPI,
                                    blink::features::kFileHandlingIcons},
                                   {});
    WebAppFileHandlerManager::SetIconsSupportedByOsForTesting(GetParam());
  }
  ~WebAppFileHandlingIconBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(WebAppFileHandlingIconBrowserTest, Basic) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL app_url(
      embedded_test_server()->GetURL("/web_app_file_handling/icons_app.html"));
  const AppId app_id = InstallWebAppFromManifest(browser(), app_url);
  auto* provider = WebAppProvider::GetForTest(browser()->profile());
  const WebApp* web_app = provider->registrar().GetAppById(app_id);
  ASSERT_TRUE(web_app);

  ASSERT_EQ(1U, web_app->file_handlers().size());
  if (WebAppFileHandlerManager::IconsEnabled()) {
    ASSERT_EQ(1U, web_app->file_handlers()[0].downloaded_icons.size());
    EXPECT_EQ(20,
              web_app->file_handlers()[0].downloaded_icons[0].square_size_px);
  } else {
    EXPECT_TRUE(web_app->file_handlers()[0].downloaded_icons.empty());
  }
}

// TODO(crbug.com/1218210): add more tests.

INSTANTIATE_TEST_SUITE_P(, WebAppFileHandlingIconBrowserTest, testing::Bool());

// The following fixtures help test what happens when the feature's state
// changes between browser launches.
class WebAppFileHandlingBrowserTest_FeatureSwitchesState
    : public WebAppFileHandlingBrowserTest {
 public:
  explicit WebAppFileHandlingBrowserTest_FeatureSwitchesState(
      bool feature_switches_from_off_to_on = false) {
    if (feature_switches_from_off_to_on == content::IsPreTest()) {
      scoped_feature_list_.InitAndDisableFeature(
          blink::features::kFileHandlingAPI);
    }

    WebAppFileHandlerManager::DisableOsIntegrationForTesting(
        base::BindRepeating(
            &WebAppFileHandlingBrowserTest_FeatureSwitchesState::
                IntegrationWasSet,
            base::Unretained(this)));
  }
  ~WebAppFileHandlingBrowserTest_FeatureSwitchesState() override = default;

  void InstallApp() {
    EXPECT_EQ(0u, registrar().GetAppIds().size());
    InstallFileHandlingPWA();
    EXPECT_EQ(1u, registrar().GetAppIds().size());

    // `InstallFileHandlingPWA()` doesn't perform OS integration, so explicitly
    // call it here to simulate a user install. Note: does nothing if the
    // feature is disabled.
    file_handler_manager().EnableAndRegisterOsFileHandlers(app_id());
  }

  void IntegrationWasSet(bool enabled) {
    if (enabled)
      added_count_++;
    else
      removed_count_++;
  }

 protected:
  // The number of times Chrome has called into the OS to set state.
  int added_count_ = 0;
  int removed_count_ = 0;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// TODO(crbug/1288442): re-enable these tests
#if !BUILDFLAG(IS_CHROMEOS)
// This test fixture will run the PRE_ test with the feature disabled, then the
// main test with the feature enabled. If a FH app was installed when the
// feature was disabled, then the feature becomes enabled, it should be
// registered with the OS.
class WebAppFileHandlingBrowserTest_FeatureSwitchesOn
    : public WebAppFileHandlingBrowserTest_FeatureSwitchesState {
 public:
  WebAppFileHandlingBrowserTest_FeatureSwitchesOn()
      : WebAppFileHandlingBrowserTest_FeatureSwitchesState(
            /*feature_switches_from_off_to_on=*/true) {}
  ~WebAppFileHandlingBrowserTest_FeatureSwitchesOn() override = default;
};

IN_PROC_BROWSER_TEST_F(WebAppFileHandlingBrowserTest_FeatureSwitchesOn,
                       PRE_PRE_OsIntegrationIsAdded) {
  InstallApp();
  EXPECT_FALSE(file_handler_manager().IsFileHandlingAPIAvailable(app_id()));
  EXPECT_FALSE(registrar().ExpectThatFileHandlersAreRegisteredWithOs(app_id()));
  EXPECT_EQ(0, added_count_);
  EXPECT_EQ(0, removed_count_);
}

IN_PROC_BROWSER_TEST_F(WebAppFileHandlingBrowserTest_FeatureSwitchesOn,
                       PRE_OsIntegrationIsAdded) {
  // In this intermediate test, the feature is still off. Verify that Chrome
  // doesn't make excess calls to the OS.
  EXPECT_EQ(0, added_count_);
  EXPECT_EQ(0, removed_count_);
}

IN_PROC_BROWSER_TEST_F(WebAppFileHandlingBrowserTest_FeatureSwitchesOn,
                       OsIntegrationIsAdded) {
  ASSERT_EQ(1u, registrar().GetAppIds().size());
  AppId app_id = registrar().GetAppIds()[0];
  EXPECT_TRUE(file_handler_manager().IsFileHandlingAPIAvailable(app_id));
  EXPECT_TRUE(registrar().ExpectThatFileHandlersAreRegisteredWithOs(app_id));
  EXPECT_EQ(1, added_count_);
  EXPECT_EQ(0, removed_count_);
}

// This test fixture verifies the opposite of the above. It will run the PRE_
// test with the feature enabled, then the main test with the feature disabled.
// If a FH app was installed when the feature was enabled, then the feature
// becomes disabled, it should be no longer be registered with the OS.
class WebAppFileHandlingBrowserTest_FeatureSwitchesOff
    : public WebAppFileHandlingBrowserTest_FeatureSwitchesState {
 public:
  WebAppFileHandlingBrowserTest_FeatureSwitchesOff()
      : WebAppFileHandlingBrowserTest_FeatureSwitchesState(
            /*feature_switches_from_off_to_on=*/false) {}
  ~WebAppFileHandlingBrowserTest_FeatureSwitchesOff() override = default;
};

IN_PROC_BROWSER_TEST_F(WebAppFileHandlingBrowserTest_FeatureSwitchesOff,
                       PRE_OsIntegrationIsRemoved) {
  InstallApp();
  EXPECT_TRUE(file_handler_manager().IsFileHandlingAPIAvailable(app_id()));
  EXPECT_TRUE(registrar().ExpectThatFileHandlersAreRegisteredWithOs(app_id()));
  EXPECT_EQ(1, added_count_);
  EXPECT_EQ(0, removed_count_);
}

IN_PROC_BROWSER_TEST_F(WebAppFileHandlingBrowserTest_FeatureSwitchesOff,
                       OsIntegrationIsRemoved) {
  ASSERT_EQ(1u, registrar().GetAppIds().size());
  AppId app_id = registrar().GetAppIds()[0];
  EXPECT_FALSE(file_handler_manager().IsFileHandlingAPIAvailable(app_id));
  EXPECT_FALSE(registrar().ExpectThatFileHandlersAreRegisteredWithOs(app_id));
  EXPECT_EQ(0, added_count_);
  EXPECT_EQ(1, removed_count_);
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

}  // namespace web_app
