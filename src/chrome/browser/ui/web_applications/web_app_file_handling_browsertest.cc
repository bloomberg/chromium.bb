// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
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
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/file_handler_manager.h"
#include "chrome/browser/web_applications/components/os_integration_manager.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_prefs_utils.h"
#include "chrome/browser/web_applications/components/web_app_provider_base.h"
#include "chrome/browser/web_applications/components/web_app_utils.h"
#include "chrome/browser/web_applications/components/web_application_info.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/embedder_support/switches.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/url_loader_interceptor.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/web_launch/file_handling_expiry.mojom-test-utils.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/chromeos/file_manager/file_manager_test_util.h"
#endif

namespace web_app {

// A fake file handling expiry service. This service allows us to mock having an
// origin trial which expires having a certain time, without needing to manage
// actual origin trial tokens.
class FakeFileHandlingExpiryService
    : public blink::mojom::FileHandlingExpiryInterceptorForTesting {
 public:
  FakeFileHandlingExpiryService()
      : expiry_time_(base::Time::Now() + base::TimeDelta::FromDays(1)) {}

  blink::mojom::FileHandlingExpiry* GetForwardingInterface() override {
    NOTREACHED();
    return nullptr;
  }

  void Bind(mojo::ScopedInterfaceEndpointHandle handle) {
    receiver_.reset();
    receiver_.Bind(
        mojo::PendingAssociatedReceiver<blink::mojom::FileHandlingExpiry>(
            std::move(handle)));
  }

  void SetExpiryTime(base::Time expiry_time) { expiry_time_ = expiry_time; }

  void RequestOriginTrialExpiryTime(
      RequestOriginTrialExpiryTimeCallback callback) override {
    if (before_reply_callback_) {
      std::move(before_reply_callback_).Run();
    }

    std::move(callback).Run(expiry_time_);
  }

  // Set a callback to be called before FileHandlingExpiry interface replies
  // the expiry time. Useful for testing inflight IPC.
  void SetBeforeReplyCallback(base::RepeatingClosure before_reply_callback) {
    before_reply_callback_ = before_reply_callback;
  }

 private:
  base::Time expiry_time_;
  RequestOriginTrialExpiryTimeCallback callback_;
  base::RepeatingClosure before_reply_callback_;
  mojo::AssociatedReceiver<blink::mojom::FileHandlingExpiry> receiver_{this};
};

class WebAppFileHandlingTestBase : public WebAppControllerBrowserTest {
 public:
  WebAppProviderBase* provider() {
    return WebAppProviderBase::GetProviderBase(profile());
  }

  FileHandlerManager& file_handler_manager() {
    return provider()
        ->os_integration_manager()
        .file_handler_manager_for_testing();
  }

  AppRegistrar& registrar() { return provider()->registrar(); }

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

    auto web_app_info = std::make_unique<WebApplicationInfo>();
    web_app_info->start_url = url;
    web_app_info->scope = url.GetWithoutFilename();
    web_app_info->title = u"A Hosted App";

    // Basic plain text format.
    blink::Manifest::FileHandler entry1;
    entry1.action = GetTextFileHandlerActionURL();
    entry1.name = u"text";
    entry1.accept[u"text/*"].push_back(u".txt");
    web_app_info->file_handlers.push_back(std::move(entry1));

    // A format that the browser is also a handler for, to confirm that the
    // browser doesn't override PWAs using File Handling for types that the
    // browser also handles.
    blink::Manifest::FileHandler entry2;
    entry2.action = GetHTMLFileHandlerActionURL();
    entry2.name = u"html";
    entry2.accept[u"text/html"].push_back(u".html");
    web_app_info->file_handlers.push_back(std::move(entry2));

    // application/* format.
    blink::Manifest::FileHandler entry3;
    entry3.action = GetCSVFileHandlerActionURL();
    entry3.name = u"csv";
    entry3.accept[u"application/csv"].push_back(u".csv");
    web_app_info->file_handlers.push_back(std::move(entry3));

    app_id_ =
        WebAppControllerBrowserTest::InstallWebApp(std::move(web_app_info));
  }

  void InstallAnotherFileHandlingPwa(const GURL& url) {
    auto web_app_info = std::make_unique<WebApplicationInfo>();
    web_app_info->start_url = url;
    web_app_info->scope = url.GetWithoutFilename();
    web_app_info->title = u"A second app";

    // This one handles jpegs.
    blink::Manifest::FileHandler entry1;
    entry1.action = GetTextFileHandlerActionURL();
    entry1.name = u"jpeg";
    entry1.accept[u"image/jpeg"].push_back(u".jpeg");
    web_app_info->file_handlers.push_back(std::move(entry1));

    WebAppControllerBrowserTest::InstallWebApp(std::move(web_app_info));
  }

 protected:
  void SetFileHandlingPermission(ContentSetting setting) {
    auto* map =
        HostContentSettingsMapFactory::GetForProfile(browser()->profile());
    map->SetDefaultContentSetting(ContentSettingsType::FILE_HANDLING, setting);
  }

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

content::WebContents* LaunchApplication(
    Profile* profile,
    const std::string& app_id,
    const GURL& expected_launch_url,
    const apps::mojom::LaunchContainer launch_container =
        apps::mojom::LaunchContainer::kLaunchContainerWindow,
    const apps::mojom::AppLaunchSource launch_source =
        apps::mojom::AppLaunchSource::kSourceTest,
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
          ->LaunchAppWithParams(std::move(params));

  navigation_observer.Wait();

  // Attach the launchParams to the window so we can inspect them easily.
  auto result = content::EvalJs(web_contents,
                                "launchQueue.setConsumer(launchParams => {"
                                "  window.launchParams = launchParams;"
                                "});");

  return web_contents;
}

}  // namespace

class WebAppFileHandlingBrowserTest : public WebAppFileHandlingTestBase {
 public:
  WebAppFileHandlingBrowserTest() {
    scoped_feature_list_.InitWithFeatures({blink::features::kFileHandlingAPI},
                                          {});
  }
  content::WebContents* LaunchWithFiles(
      const std::string& app_id,
      const GURL& expected_launch_url,
      const std::vector<base::FilePath>& files,
      const apps::mojom::LaunchContainer launch_container =
          apps::mojom::LaunchContainer::kLaunchContainerWindow) {
    return LaunchApplication(
        profile(), app_id, expected_launch_url, launch_container,
        apps::mojom::AppLaunchSource::kSourceFileHandler, files);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(WebAppFileHandlingBrowserTest,
                       LaunchConsumerIsNotTriggeredWithNoFiles) {
  InstallFileHandlingPWA();
  SetFileHandlingPermission(CONTENT_SETTING_ALLOW);
  content::WebContents* web_contents =
      LaunchWithFiles(app_id(), GetSecureAppURL(), {});
  EXPECT_EQ(false, content::EvalJs(web_contents, "!!window.launchParams"));
}

IN_PROC_BROWSER_TEST_F(WebAppFileHandlingBrowserTest,
                       PWAsCanReceiveFileLaunchParams) {
  InstallFileHandlingPWA();
  SetFileHandlingPermission(CONTENT_SETTING_ALLOW);
  base::FilePath test_file_path = NewTestFilePath("txt");
  content::WebContents* web_contents = LaunchWithFiles(
      app_id(), GetTextFileHandlerActionURL(), {test_file_path});

  EXPECT_EQ(1,
            content::EvalJs(web_contents, "window.launchParams.files.length"));
  EXPECT_EQ(test_file_path.BaseName().AsUTF8Unsafe(),
            content::EvalJs(web_contents, "window.launchParams.files[0].name"));
}

IN_PROC_BROWSER_TEST_F(WebAppFileHandlingBrowserTest,
                       LaunchConsumerIsNotTriggeredWithPermissionDenied) {
  InstallFileHandlingPWA();
  SetFileHandlingPermission(CONTENT_SETTING_BLOCK);
  base::FilePath test_file_path = NewTestFilePath("txt");
  content::WebContents* web_contents = LaunchWithFiles(
      app_id(), GetTextFileHandlerActionURL(), {test_file_path});

  EXPECT_EQ(false, content::EvalJs(web_contents, "!!window.launchParams"));
}

IN_PROC_BROWSER_TEST_F(WebAppFileHandlingBrowserTest,
                       PWAsCanReceiveFileLaunchParamsInTab) {
  InstallFileHandlingPWA();
  SetFileHandlingPermission(CONTENT_SETTING_ALLOW);
  base::FilePath test_file_path = NewTestFilePath("txt");
  content::WebContents* web_contents =
      LaunchWithFiles(app_id(), GetTextFileHandlerActionURL(), {test_file_path},
                      apps::mojom::LaunchContainer::kLaunchContainerTab);

  EXPECT_EQ(1,
            content::EvalJs(web_contents, "window.launchParams.files.length"));
  EXPECT_EQ(test_file_path.BaseName().AsUTF8Unsafe(),
            content::EvalJs(web_contents, "window.launchParams.files[0].name"));
}

IN_PROC_BROWSER_TEST_F(WebAppFileHandlingBrowserTest,
                       PWAsDispatchOnCorrectFileHandlingURL) {
  InstallFileHandlingPWA();
  SetFileHandlingPermission(CONTENT_SETTING_ALLOW);

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

// Disabled due to flakiness on Linux bots. http://crbug.com/1207370
#if defined(OS_LINUX)
#define MAYBE_UnlimitedFileHandlersForChrome \
  DISABLED_UnlimitedFileHandlersForChrome
#else
#define MAYBE_UnlimitedFileHandlersForChrome UnlimitedFileHandlersForChrome
#endif
IN_PROC_BROWSER_TEST_F(WebAppFileHandlingBrowserTest,
                       MAYBE_UnlimitedFileHandlersForChrome) {
  // We install more than |kMaxFileHandlers| file handlers.
  const unsigned kNumHandlers = 2 * kMaxFileHandlers + 1;

  auto action_url = [](unsigned index) {
    return GURL(base::StringPrintf("chrome://interstitials/#a%u", index));
  };

  auto mime_type = [](unsigned index) {
    return base::StringPrintf("application/x-%u", index);
  };

  auto extension = [](unsigned index) {
    return base::StringPrintf(".e%u", index);
  };

  AppId app_id;
  {
    auto web_app_info = std::make_unique<WebApplicationInfo>();
    web_app_info->start_url = GURL("chrome://interstitials/");
    web_app_info->scope = web_app_info->start_url;
    web_app_info->title = u"Many File Handlers";

    for (unsigned i = 0; i < kNumHandlers; ++i) {
      const std::u16string name =
          base::UTF8ToUTF16(base::StringPrintf("n%u", i));
      std::map<std::u16string, std::vector<std::u16string>> accept;
      accept[base::UTF8ToUTF16(mime_type(i))] = {
          base::UTF8ToUTF16(extension(i))};
      web_app_info->file_handlers.push_back(
          {action_url(i), name, std::move(accept)});
    }

    app_id =
        WebAppControllerBrowserTest::InstallWebApp(std::move(web_app_info));
  }

  EXPECT_EQ(registrar()
                .AsWebAppRegistrar()
                ->GetAppById(app_id)
                ->file_handlers()
                .size(),
            kNumHandlers);

  SetFileHandlingPermission(CONTENT_SETTING_ALLOW);

  // Test that file handler dispatches correct URL based on file extension.
  for (unsigned i = 0; i < kNumHandlers; ++i) {
    LaunchWithFiles(app_id, action_url(i), {NewTestFilePath(extension(i))});
  }
}

// Tests that when two apps are installed and share an origin (but not scope),
// `GetFileHandlersForAllWebAppsWithOrigin` will report all the file handlers
// across both apps.
IN_PROC_BROWSER_TEST_F(WebAppFileHandlingBrowserTest,
                       FileHandlerAggregationForUi) {
  InstallFileHandlingPWA();
  EXPECT_EQ(3U,
            GetFileHandlersForAllWebAppsWithOrigin(profile(), GetSecureAppURL())
                .size());

  GURL second_app_url = https_server()->GetURL("app.com", "/pwa/app2.html");
  InstallAnotherFileHandlingPwa(second_app_url);
  EXPECT_EQ(2U, registrar().GetAppIds().size());
  EXPECT_EQ(4U,
            GetFileHandlersForAllWebAppsWithOrigin(profile(), GetSecureAppURL())
                .size());
  EXPECT_EQ(
      4U,
      GetFileHandlersForAllWebAppsWithOrigin(profile(), second_app_url).size());

  std::u16string display_string_app1 =
      GetFileTypeAssociationsHandledByWebAppsForDisplay(profile(),
                                                        GetSecureAppURL());
  std::u16string display_string_app2 =
      GetFileTypeAssociationsHandledByWebAppsForDisplay(profile(),
                                                        second_app_url);
  EXPECT_EQ(display_string_app1, display_string_app2);
#if defined(OS_LINUX)
  const std::u16string kHtmlDisplayString = u"text/html";
  const std::u16string kJpegDisplayString = u"image/jpeg";
#else
  const std::u16string kHtmlDisplayString = u"HTML";
  const std::u16string kJpegDisplayString = u"JPEG";
#endif
  EXPECT_NE(std::u16string::npos, display_string_app1.find(kHtmlDisplayString));
  EXPECT_NE(std::u16string::npos, display_string_app1.find(kJpegDisplayString));
}

IN_PROC_BROWSER_TEST_F(WebAppFileHandlingBrowserTest,
                       SometimesResetPermission) {
  // Install the first app and simulate the user granting it the file handling
  // permission.
  InstallFileHandlingPWA();
  auto* map = HostContentSettingsMapFactory::GetForProfile(profile());
  const GURL origin = GetSecureAppURL().GetOrigin();
  EXPECT_EQ(CONTENT_SETTING_ASK,
            map->GetContentSetting(origin, origin,
                                   ContentSettingsType::FILE_HANDLING));
  map->SetContentSettingDefaultScope(origin, origin,
                                     ContentSettingsType::FILE_HANDLING,
                                     CONTENT_SETTING_ALLOW);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            map->GetContentSetting(origin, origin,
                                   ContentSettingsType::FILE_HANDLING));

  // Install a second app, which is on the same origin and asks to handle more
  // file types. The permission should have been set back to ASK.
  GURL second_app_url = https_server()->GetURL("app.com", "/pwa/app2.html");
  InstallAnotherFileHandlingPwa(second_app_url);
  EXPECT_EQ(CONTENT_SETTING_ASK,
            map->GetContentSetting(origin, origin,
                                   ContentSettingsType::FILE_HANDLING));

  // Set to ALLOW again.
  map->SetContentSettingDefaultScope(origin, origin,
                                     ContentSettingsType::FILE_HANDLING,
                                     CONTENT_SETTING_ALLOW);

  // Install a third app, which is on a different origin; this should have no
  // effect on the permission.
  GURL third_app_url = https_server()->GetURL("otherapp.com", "/pwa/app2.html");
  InstallAnotherFileHandlingPwa(third_app_url);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            map->GetContentSetting(origin, origin,
                                   ContentSettingsType::FILE_HANDLING));
  GURL third_app_origin = third_app_url.GetOrigin();
  EXPECT_EQ(CONTENT_SETTING_ASK,
            map->GetContentSetting(third_app_origin, third_app_origin,
                                   ContentSettingsType::FILE_HANDLING));
  // Install a fourth app, which is on the same origin but asks for a subset of
  // the file types of the first two. This should have no effect on the
  // permission.
  GURL fourth_app_url = https_server()->GetURL("app.com", "/pwa2/app2.html");
  InstallAnotherFileHandlingPwa(fourth_app_url);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            map->GetContentSetting(origin, origin,
                                   ContentSettingsType::FILE_HANDLING));
}

class WebAppFileHandlingOriginTrialBrowserTest
    : public WebAppFileHandlingTestBase {
 public:
  WebAppFileHandlingOriginTrialBrowserTest() {
    FileHandlerManager::DisableAutomaticFileHandlerCleanupForTesting();
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  void SetUpOnMainThread() override {
    WebAppFileHandlingTestBase::SetUpOnMainThread();
  }

  void SetUpInterceptorNavigateToAppAndMaybeWait() {
    base::RunLoop loop;
    file_handler_manager().SetOnFileHandlingExpiryUpdatedForTesting(
        loop.QuitClosure());
    web_contents()
        ->GetMainFrame()
        ->GetRemoteAssociatedInterfaces()
        ->OverrideBinderForTesting(
            blink::mojom::FileHandlingExpiry::Name_,
            base::BindRepeating(&FakeFileHandlingExpiryService::Bind,
                                base::Unretained(&file_handling_expiry_)));
    NavigateInRenderer(web_contents(), GetSecureAppURL());

    // The expiry time is only updated if the app is installed.
    if (registrar().IsInstalled(app_id()))
      loop.Run();
  }

 protected:
  FakeFileHandlingExpiryService& file_handling_expiry() {
    return file_handling_expiry_;
  }

 private:
  FakeFileHandlingExpiryService file_handling_expiry_;
};

IN_PROC_BROWSER_TEST_F(WebAppFileHandlingOriginTrialBrowserTest,
                       FileHandlingIsNotAvailableUntilOriginTrialIsChecked) {
  InstallFileHandlingPWA();

  // We haven't navigated to the app, so we don't know if it's allowed to handle
  // files.
  EXPECT_FALSE(file_handler_manager().IsFileHandlingAPIAvailable(app_id()));
  EXPECT_FALSE(file_handler_manager().AreFileHandlersEnabled(app_id()));

  // Navigating to the app should update the origin trial expiry (and allow it
  // to handle files).
  SetUpInterceptorNavigateToAppAndMaybeWait();

  EXPECT_TRUE(file_handler_manager().IsFileHandlingAPIAvailable(app_id()));
  EXPECT_TRUE(file_handler_manager().AreFileHandlersEnabled(app_id()));
}

IN_PROC_BROWSER_TEST_F(WebAppFileHandlingOriginTrialBrowserTest,
                       FileHandlingOriginTrialIsCheckedAtInstallation) {
  // Navigate to the app's launch url, so the origin trial token can be checked.
  SetUpInterceptorNavigateToAppAndMaybeWait();

  base::RunLoop loop;
  file_handler_manager().SetOnFileHandlingExpiryUpdatedForTesting(
      loop.QuitClosure());
  InstallFileHandlingPWA();
  loop.Run();

  EXPECT_TRUE(file_handler_manager().IsFileHandlingAPIAvailable(app_id()));
  EXPECT_TRUE(file_handler_manager().AreFileHandlersEnabled(app_id()));
}

IN_PROC_BROWSER_TEST_F(WebAppFileHandlingOriginTrialBrowserTest,
                       WhenOriginTrialHasExpiredFileHandlersAreNotAvailable) {
  InstallFileHandlingPWA();
  SetUpInterceptorNavigateToAppAndMaybeWait();
  EXPECT_TRUE(file_handler_manager().AreFileHandlersEnabled(app_id()));

  // Set the token's expiry to some time in the past.
  file_handling_expiry().SetExpiryTime(base::Time());

  // Refresh the page, to receive the updated expiry time.
  SetUpInterceptorNavigateToAppAndMaybeWait();
  EXPECT_FALSE(file_handler_manager().AreFileHandlersEnabled(app_id()));
  EXPECT_FALSE(file_handler_manager().GetEnabledFileHandlers(app_id()));
}

// Tests that expired file handlers are cleaned up.
// Part 1: Install a file handling app and set it's expiry time to some time in
// the past.
IN_PROC_BROWSER_TEST_F(WebAppFileHandlingOriginTrialBrowserTest,
                       PRE_ExpiredTrialHandlersAreCleanedUpAtLaunch) {
  InstallFileHandlingPWA();
  SetUpInterceptorNavigateToAppAndMaybeWait();
  EXPECT_TRUE(file_handler_manager().AreFileHandlersEnabled(app_id()));

  // Update the expiry time to be in the past.
  UpdateDoubleWebAppPref(profile()->GetPrefs(), app_id(),
                         kFileHandlingOriginTrialExpiryTime,
                         base::Time().ToDoubleT());
}

// Part 2: Test that expired file handlers for an app are cleaned up.
IN_PROC_BROWSER_TEST_F(WebAppFileHandlingOriginTrialBrowserTest,
                       ExpiredTrialHandlersAreCleanedUpAtLaunch) {
  EXPECT_EQ(1, file_handler_manager().TriggerFileHandlerCleanupForTesting());
}

// Tests that non expired file handlers are not cleaned up.
// Part 1: Install an app with valid file handlers.
IN_PROC_BROWSER_TEST_F(WebAppFileHandlingOriginTrialBrowserTest,
                       PRE_ValidFileHandlerAreNotCleanedUpAtLaunch) {
  InstallFileHandlingPWA();
  SetUpInterceptorNavigateToAppAndMaybeWait();
  EXPECT_TRUE(file_handler_manager().AreFileHandlersEnabled(app_id()));
}

// Part 2: Test that expired file handlers for an app are cleaned up.
IN_PROC_BROWSER_TEST_F(WebAppFileHandlingOriginTrialBrowserTest,
                       ValidFileHandlerAreNotCleanedUpAtLaunch) {
  EXPECT_EQ(0, file_handler_manager().TriggerFileHandlerCleanupForTesting());
}

IN_PROC_BROWSER_TEST_F(WebAppFileHandlingOriginTrialBrowserTest,
                       DisableForceEnabledFileHandlingOriginTrial) {
  InstallFileHandlingPWA();
  SetUpInterceptorNavigateToAppAndMaybeWait();

  ASSERT_TRUE(file_handler_manager().AreFileHandlersEnabled(app_id()));
  ASSERT_TRUE(file_handler_manager().GetEnabledFileHandlers(app_id()));

  // Calling this on non-force-enabled origin trial should have no effect.
  file_handler_manager().DisableForceEnabledFileHandlingOriginTrial(app_id());
  EXPECT_TRUE(file_handler_manager().AreFileHandlersEnabled(app_id()));
  EXPECT_TRUE(file_handler_manager().GetEnabledFileHandlers(app_id()));

  // Force enables file handling.
  file_handler_manager().ForceEnableFileHandlingOriginTrial(app_id());

  // Calling this on force enabled origin trial should remove file handlers.
  file_handler_manager().DisableForceEnabledFileHandlingOriginTrial(app_id());
  EXPECT_FALSE(file_handler_manager().AreFileHandlersEnabled(app_id()));
  EXPECT_EQ(nullptr, file_handler_manager().GetEnabledFileHandlers(app_id()));
}

IN_PROC_BROWSER_TEST_F(WebAppFileHandlingOriginTrialBrowserTest,
                       ForceEnabledFileHandling_IgnoreExpiryTimeUpdate) {
  InstallFileHandlingPWA();
  SetUpInterceptorNavigateToAppAndMaybeWait();

  EXPECT_FALSE(file_handler_manager().IsFileHandlingForceEnabled(app_id()));

  // Force enables file handling.
  file_handler_manager().ForceEnableFileHandlingOriginTrial(app_id());
  EXPECT_TRUE(file_handler_manager().IsFileHandlingForceEnabled(app_id()));

  // Update origin trial expiry time from the App's WebContents.
  base::RunLoop loop;
  file_handler_manager().SetOnFileHandlingExpiryUpdatedForTesting(
      loop.QuitClosure());
  file_handling_expiry().SetExpiryTime(base::Time());
  file_handler_manager().MaybeUpdateFileHandlingOriginTrialExpiry(
      web_contents(), app_id());
  loop.Run();

  // Force enabled file handling should not be updated by the expiry time in
  // App's WebContents (i.e. origin trial token expiry).
  EXPECT_TRUE(file_handler_manager().IsFileHandlingForceEnabled(app_id()));
  EXPECT_TRUE(file_handler_manager().IsFileHandlingAPIAvailable(app_id()));
  EXPECT_TRUE(file_handler_manager().GetEnabledFileHandlers(app_id()));
}

IN_PROC_BROWSER_TEST_F(WebAppFileHandlingOriginTrialBrowserTest,
                       ForceEnabledFileHandling_IgnoreExpiryTimeInflightIPC) {
  InstallFileHandlingPWA();
  SetUpInterceptorNavigateToAppAndMaybeWait();

  EXPECT_FALSE(file_handler_manager().IsFileHandlingForceEnabled(app_id()));

  // Request to update origin trial expiry time from the App's WebContents, and
  // force enables file handling origin trial before the expiry time reply is
  // received.
  base::RunLoop loop;
  file_handler_manager().SetOnFileHandlingExpiryUpdatedForTesting(
      loop.QuitClosure());
  file_handling_expiry().SetExpiryTime(base::Time());
  file_handling_expiry().SetBeforeReplyCallback(
      base::BindLambdaForTesting([&]() {
        EXPECT_FALSE(
            file_handler_manager().IsFileHandlingForceEnabled(app_id()));
        file_handler_manager().ForceEnableFileHandlingOriginTrial(app_id());
      }));

  EXPECT_FALSE(file_handler_manager().IsFileHandlingForceEnabled(app_id()));
  file_handler_manager().MaybeUpdateFileHandlingOriginTrialExpiry(
      web_contents(), app_id());
  loop.Run();

  // Force enabled file handling should not be updated by the inflight expiry
  // time IPC.
  EXPECT_TRUE(file_handler_manager().IsFileHandlingForceEnabled(app_id()));
  EXPECT_TRUE(file_handler_manager().IsFileHandlingAPIAvailable(app_id()));
  EXPECT_TRUE(file_handler_manager().GetEnabledFileHandlers(app_id()));
}

namespace {
constexpr char kBaseDataDir[] = "chrome/test/data/web_app_file_handling";

// This is the public key of tools/origin_trials/eftest.key, used to validate
// origin trial tokens generated by tools/origin_trials/generate_token.py.
constexpr char kOriginTrialPublicKeyForTesting[] =
    "dRCs+TocuKkocNKa0AtZ4awrt9XKH2SQCI6o4FY6BNA=";

}  // namespace

class WebAppFileHandlingOriginTrialTest : public WebAppControllerBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    WebAppControllerBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(embedder_support::kOriginTrialPublicKey,
                                    kOriginTrialPublicKeyForTesting);
  }

  void TearDownOnMainThread() override { interceptor_.reset(); }

 protected:
  void GrantFileHandlingPermission() {
    auto* map =
        HostContentSettingsMapFactory::GetForProfile(browser()->profile());
    map->SetDefaultContentSetting(ContentSettingsType::FILE_HANDLING,
                                  CONTENT_SETTING_ALLOW);
  }

  AppId InstallFileHandlingWebApp(GURL* start_url_out = nullptr) {
    std::string origin = "https://file-handling-pwa";

    // We need to use URLLoaderInterceptor (rather than a EmbeddedTestServer),
    // because origin trial token is associated with a fixed origin, whereas
    // EmbeddedTestServer serves content on a random port.
    interceptor_ =
        content::URLLoaderInterceptor::ServeFilesFromDirectoryAtOrigin(
            kBaseDataDir, GURL(origin));

    GURL start_url = GURL(origin + "/index.html");

    auto web_app_info = std::make_unique<WebApplicationInfo>();
    web_app_info->start_url = start_url;
    web_app_info->scope = start_url.GetWithoutFilename();
    web_app_info->title = u"A Web App";

    blink::Manifest::FileHandler entry1;
    entry1.action = start_url;
    entry1.name = u"text";
    entry1.accept[u"text/*"].push_back(u".txt");
    web_app_info->file_handlers.push_back(std::move(entry1));

    AppId app_id =
        WebAppControllerBrowserTest::InstallWebApp(std::move(web_app_info));

    // Here we need first launch the App, so it can update the origin trial
    // expiry time in prefs. This is needed because the above InstallWebApp
    // invocation bypassed the normal Web App install pipeline.
    content::WebContents* web_content =
        LaunchApplication(profile(), app_id, start_url);
    web_content->Close();

    if (start_url_out)
      *start_url_out = start_url;
    return app_id;
  }

 private:
  std::unique_ptr<content::URLLoaderInterceptor> interceptor_;
};

IN_PROC_BROWSER_TEST_F(WebAppFileHandlingOriginTrialTest,
                       LaunchParamsArePassedCorrectly) {
  GURL start_url;
  const AppId app_id = InstallFileHandlingWebApp(&start_url);
  GrantFileHandlingPermission();
  base::FilePath test_file_path = NewTestFilePath("txt");
  content::WebContents* web_content = LaunchApplication(
      profile(), app_id, start_url,
      apps::mojom::LaunchContainer::kLaunchContainerWindow,
      apps::mojom::AppLaunchSource::kSourceFileHandler, {test_file_path});
  EXPECT_EQ(1,
            content::EvalJs(web_content, "window.launchParams.files.length"));
  EXPECT_EQ(test_file_path.BaseName().AsUTF8Unsafe(),
            content::EvalJs(web_content, "window.launchParams.files[0].name"));
}

class WebAppFileHandlingPolicyBrowserTest
    : public WebAppFileHandlingBrowserTest {
 public:
  // Set the file handling policy to BLOCK the app between the PRE test and the
  // actual test.
  void SetUpInProcessBrowserTestFixture() override {
    if (GetTestPreCount() == 0) {
      SetFileHandlingBlockPolicy();
    }
  }

 private:
  void SetFileHandlingBlockPolicy() {
    ON_CALL(provider_, IsInitializationComplete(testing::_))
        .WillByDefault(testing::Return(true));
    ON_CALL(provider_, IsFirstPolicyLoadComplete(testing::_))
        .WillByDefault(testing::Return(true));

    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);

    policy::PolicyMap values;
    base::Value list(base::Value::Type::LIST);
    list.Append(base::Value("https://app.com"));
    policy::PolicyMap::Entry entry_list(
        policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
        policy::POLICY_SOURCE_CLOUD, std::move(list), nullptr);

    values.Set(policy::key::kFileHandlingBlockedForUrls, std::move(entry_list));
    provider_.UpdateChromePolicy(values);
  }
  testing::NiceMock<policy::MockConfigurationPolicyProvider> provider_;
};

IN_PROC_BROWSER_TEST_F(WebAppFileHandlingPolicyBrowserTest,
                       PRE_PolicySettingsBlockedUrl) {
  InstallFileHandlingPWA();
  EXPECT_EQ(registrar().GetAppIds().size(), 1u);
  EXPECT_FALSE(registrar()
                   .AsWebAppRegistrar()
                   ->GetAppById(app_id())
                   ->file_handler_permission_blocked());
}

// Test that the app's `file_handler_permission_blocked` state should be updated
// on WebAppProvider system setup based on current permission settings.
IN_PROC_BROWSER_TEST_F(WebAppFileHandlingPolicyBrowserTest,
                       PolicySettingsBlockedUrl) {
  auto* provider = WebAppProvider::Get(profile());
  DCHECK(provider);
  test::WaitUntilReady(provider);

  std::vector<AppId> app_ids = registrar().GetAppIds();
  EXPECT_EQ(app_ids.size(), 1u);
  EXPECT_TRUE(registrar()
                  .AsWebAppRegistrar()
                  ->GetAppById(app_ids[0])
                  ->file_handler_permission_blocked());
}

#if BUILDFLAG(IS_CHROMEOS_ASH)

// End-to-end test to ensure the file handler is registered on ChromeOS when the
// extension system is initialized. Gives more coverage than the unit tests for
// web_file_tasks.cc.
IN_PROC_BROWSER_TEST_F(WebAppFileHandlingOriginTrialTest,
                       IsFileHandlerOnChromeOS) {
  const AppId app_id = InstallFileHandlingWebApp();
  GrantFileHandlingPermission();
  base::FilePath test_file_path = NewTestFilePath("txt");
  std::vector<file_manager::file_tasks::FullTaskDescriptor> tasks =
      file_manager::test::GetTasksForFile(profile(), test_file_path);

  // Note that there are normally multiple tasks due to default-installed
  // handlers (e.g. add to zip file). But those handlers are not installed by
  // default in browser tests.
  EXPECT_EQ(1u, tasks.size());
  EXPECT_EQ(tasks[0].task_descriptor().app_id, app_id);
}

// Ensures correct behavior for files on "special volumes", such as file systems
// provided by extensions. These do not have local files (i.e. backed by
// inodes).
IN_PROC_BROWSER_TEST_F(WebAppFileHandlingOriginTrialTest,
                       NotHandlerForNonNativeFiles) {
  const AppId app_id = InstallFileHandlingWebApp();
  GrantFileHandlingPermission();
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

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace web_app
