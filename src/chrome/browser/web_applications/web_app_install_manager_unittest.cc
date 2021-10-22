// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_install_manager.h"

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/compiler_specific.h"
#include "base/containers/flat_set.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/web_applications/externally_installed_web_app_prefs.h"
#include "chrome/browser/web_applications/policy/web_app_policy_manager.h"
#include "chrome/browser/web_applications/test/fake_data_retriever.h"
#include "chrome/browser/web_applications/test/fake_web_app_database_factory.h"
#include "chrome/browser/web_applications/test/fake_web_app_registry_controller.h"
#include "chrome/browser/web_applications/test/fake_web_app_ui_manager.h"
#include "chrome/browser/web_applications/test/test_file_utils.h"
#include "chrome/browser/web_applications/test/test_web_app_url_loader.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_sync_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_icon_generator.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_install_task.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/browser/web_applications/web_application_info.h"
#include "chrome/test/base/testing_profile.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom-shared.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "third_party/skia/include/core/SkColor.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/common/chrome_features.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace web_app {

namespace {

constexpr SquareSizePx kDefaultImageSize = 100;
constexpr char kIconUrl[] = "https://example.com/app.ico";

std::unique_ptr<WebApplicationInfo> ConvertWebAppToRendererWebApplicationInfo(
    const WebApp& app) {
  auto web_application_info = std::make_unique<WebApplicationInfo>();
  // Most fields are expected to be populated by a manifest data in a subsequent
  // override install process data flow. TODO(loyso): Make it more robust.
  web_application_info->description = base::UTF8ToUTF16(app.description());
  // |user_display_mode| is a user's display mode value and it is typically
  // populated by a UI dialog in production code. We set it here for testing
  // purposes.
  web_application_info->user_display_mode = app.user_display_mode();
  return web_application_info;
}

std::vector<blink::Manifest::ImageResource> ConvertWebAppIconsToImageResources(
    const WebApp& app) {
  std::vector<blink::Manifest::ImageResource> icons;
  for (const apps::IconInfo& icon_info : app.manifest_icons()) {
    blink::Manifest::ImageResource icon;
    icon.src = icon_info.url;
    // TODO(estade): remove this cast.
    icon.purpose.push_back(static_cast<IconPurpose>(icon_info.purpose));
    icon.sizes.emplace_back(
        icon_info.square_size_px.value_or(kDefaultImageSize),
        icon_info.square_size_px.value_or(kDefaultImageSize));
    icons.push_back(std::move(icon));
  }
  return icons;
}

blink::mojom::ManifestPtr ConvertWebAppToManifest(const WebApp& app) {
  auto manifest = blink::mojom::Manifest::New();
  manifest->start_url = app.start_url();
  manifest->scope = app.start_url();
  manifest->short_name = u"Short Name to be overriden.";
  manifest->name = base::UTF8ToUTF16(app.name());
  if (app.theme_color()) {
    manifest->has_theme_color = true;
    manifest->theme_color = *app.theme_color();
  }
  manifest->display = app.display_mode();
  manifest->icons = ConvertWebAppIconsToImageResources(app);
  return manifest;
}

IconsMap ConvertWebAppIconsToIconsMap(const WebApp& app) {
  IconsMap icons_map;
  for (const apps::IconInfo& icon_info : app.manifest_icons()) {
    icons_map[icon_info.url] = {CreateSquareIcon(
        icon_info.square_size_px.value_or(kDefaultImageSize), SK_ColorBLACK)};
  }
  return icons_map;
}

std::unique_ptr<WebAppDataRetriever> ConvertWebAppToDataRetriever(
    const WebApp& app) {
  auto data_retriever = std::make_unique<FakeDataRetriever>();

  data_retriever->SetRendererWebApplicationInfo(
      ConvertWebAppToRendererWebApplicationInfo(app));
  data_retriever->SetManifest(ConvertWebAppToManifest(app),
                              /*is_installable=*/true);
  data_retriever->SetIcons(ConvertWebAppIconsToIconsMap(app));

  return std::unique_ptr<WebAppDataRetriever>(std::move(data_retriever));
}

std::unique_ptr<WebAppDataRetriever> CreateEmptyDataRetriever() {
  auto data_retriever = std::make_unique<FakeDataRetriever>();
  return std::unique_ptr<WebAppDataRetriever>(std::move(data_retriever));
}

std::unique_ptr<WebAppInstallTask> CreateDummyTask() {
  return std::make_unique<WebAppInstallTask>(
      /*profile=*/nullptr,
      /*os_integration_manager=*/nullptr,
      /*install_finalizer=*/nullptr,
      /*data_retriever=*/nullptr,
      /*registrar=*/nullptr);
}

// TODO(crbug.com/1194709): Retire SyncParam after Lacros ships.
enum class SyncParam { kWithoutSync = 0, kWithSync = 1, kMaxValue = kWithSync };

}  // namespace

class WebAppInstallManagerTest
    : public WebAppTest,
      public ::testing::WithParamInterface<SyncParam> {
 public:
  WebAppInstallManagerTest() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    if (GetParam() == SyncParam::kWithSync) {
      // Disable WebAppsCrosapi, so that Web Apps get synced in the Ash browser.
      scoped_feature_list_.InitAndDisableFeature(features::kWebAppsCrosapi);
    } else {
      // Enable WebAppsCrosapi, so that Web Apps don't get synced in the Ash
      // browser.
      scoped_feature_list_.InitAndEnableFeature(features::kWebAppsCrosapi);
    }
#else
    DCHECK(GetParam() == SyncParam::kWithSync);
#endif
  }

  void SetUp() override {
    WebAppTest::SetUp();

    externally_installed_app_prefs_ =
        std::make_unique<ExternallyInstalledWebAppPrefs>(profile()->GetPrefs());

    fake_registry_controller_ =
        std::make_unique<FakeWebAppRegistryController>();
    fake_registry_controller_->SetUp(profile());

    auto file_utils = std::make_unique<TestFileUtils>();
    file_utils_ = file_utils.get();

    icon_manager_ = std::make_unique<WebAppIconManager>(profile(), registrar(),
                                                        std::move(file_utils));

    policy_manager_ = std::make_unique<WebAppPolicyManager>(profile());

    install_finalizer_ = std::make_unique<WebAppInstallFinalizer>(
        profile(), icon_manager_.get(), policy_manager_.get());

    install_manager_ = std::make_unique<WebAppInstallManager>(profile());
    install_manager_->SetSubsystems(&registrar(),
                                    &controller().os_integration_manager(),
                                    install_finalizer_.get());

    auto test_url_loader = std::make_unique<TestWebAppUrlLoader>();

    test_url_loader_ = test_url_loader.get();
    install_manager_->SetUrlLoaderForTesting(std::move(test_url_loader));

    ui_manager_ = std::make_unique<FakeWebAppUiManager>();

    install_finalizer_->SetSubsystems(
        &registrar(), ui_manager_.get(),
        &fake_registry_controller_->sync_bridge(),
        &fake_registry_controller_->os_integration_manager());
  }

  void TearDown() override {
    DestroyManagers();
    WebAppTest::TearDown();
  }

  WebAppRegistrar& registrar() { return controller().registrar(); }
  WebAppInstallManager& install_manager() { return *install_manager_; }
  WebAppInstallFinalizer& finalizer() { return *install_finalizer_; }
  WebAppIconManager& icon_manager() { return *icon_manager_; }
  TestWebAppUrlLoader& url_loader() { return *test_url_loader_; }
  TestFileUtils& file_utils() {
    DCHECK(file_utils_);
    return *file_utils_;
  }
  FakeWebAppRegistryController& controller() {
    return *fake_registry_controller_;
  }
  ExternallyInstalledWebAppPrefs& externally_installed_app_prefs() {
    return *externally_installed_app_prefs_;
  }

  std::unique_ptr<WebApp> CreateWebAppFromSyncAndPendingInstallation(
      const GURL& start_url,
      const std::string& app_name,
      DisplayMode user_display_mode,
      SkColor theme_color,
      bool locally_installed,
      const GURL& scope,
      const std::vector<apps::IconInfo>& icon_infos) {
    auto web_app = test::CreateWebApp(start_url, Source::kSync);
    web_app->SetIsFromSyncAndPendingInstallation(true);
    web_app->SetIsLocallyInstalled(locally_installed);
    web_app->SetUserDisplayMode(user_display_mode);

    WebApp::SyncFallbackData sync_fallback_data;
    sync_fallback_data.name = app_name;
    sync_fallback_data.theme_color = theme_color;
    sync_fallback_data.scope = scope;
    sync_fallback_data.icon_infos = icon_infos;
    web_app->SetSyncFallbackData(std::move(sync_fallback_data));
    return web_app;
  }

  void InitEmptyRegistrar() {
    controller().Init();
    install_finalizer_->Start();
    install_manager_->Start();
  }

  std::set<AppId> InitRegistrarWithRegistry(const Registry& registry) {
    std::set<AppId> app_ids;
    for (auto& kv : registry)
      app_ids.insert(kv.second->app_id());

    controller().database_factory().WriteRegistry(registry);

    controller().Init();
    install_finalizer_->Start();
    install_manager_->Start();

    return app_ids;
  }

  AppId InitRegistrarWithApp(std::unique_ptr<WebApp> app) {
    DCHECK(registrar().is_empty());

    const AppId app_id = app->app_id();

    Registry registry;
    registry.emplace(app_id, std::move(app));

    InitRegistrarWithRegistry(registry);
    return app_id;
  }

  struct InstallResult {
    AppId app_id;
    InstallResultCode code;
  };

  InstallResult InstallWebAppFromManifestWithFallback() {
    InstallResult result;
    base::RunLoop run_loop;
    install_manager().InstallWebAppFromManifestWithFallback(
        web_contents(), /*force_shortcut_app=*/false,
        webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON,
        base::BindOnce(test::TestAcceptDialogCallback),
        base::BindLambdaForTesting(
            [&](const AppId& installed_app_id, InstallResultCode code) {
              result.app_id = installed_app_id;
              result.code = code;
              run_loop.Quit();
            }));
    run_loop.Run();
    return result;
  }

  InstallResult InstallWebAppsAfterSync(std::vector<WebApp*> web_apps) {
    InstallResult result;
    base::RunLoop run_loop;
    install_manager().InstallWebAppsAfterSync(
        std::move(web_apps),
        base::BindLambdaForTesting(
            [&](const AppId& app_id, InstallResultCode code) {
              result.app_id = app_id;
              result.code = code;
              run_loop.Quit();
            }));
    run_loop.Run();
    return result;
  }

  InstallResult InstallWebAppFromInfo(
      std::unique_ptr<WebApplicationInfo> web_application_info,
      bool overwrite_existing_manifest_fields,
      webapps::WebappInstallSource install_source) {
    InstallResult result;
    base::RunLoop run_loop;
    install_manager().InstallWebAppFromInfo(
        std::move(web_application_info), overwrite_existing_manifest_fields,
        ForInstallableSite::kYes, install_source,
        base::BindLambdaForTesting(
            [&](const AppId& installed_app_id, InstallResultCode code) {
              result.app_id = installed_app_id;
              result.code = code;
              run_loop.Quit();
            }));
    run_loop.Run();
    return result;
  }

  std::map<SquareSizePx, SkBitmap> ReadIcons(const AppId& app_id,
                                             IconPurpose purpose,
                                             SortedSizesPx sizes_px) {
    std::map<SquareSizePx, SkBitmap> result;
    base::RunLoop run_loop;
    icon_manager().ReadIcons(
        app_id, purpose, sizes_px,
        base::BindLambdaForTesting(
            [&](std::map<SquareSizePx, SkBitmap> icon_bitmaps) {
              result = std::move(icon_bitmaps);
              run_loop.Quit();
            }));
    run_loop.Run();
    return result;
  }

  int GetNumFullyInstalledApps() const {
    int num_apps = 0;

    for (const WebApp& app : fake_registry_controller_->registrar().GetApps()) {
      ALLOW_UNUSED_LOCAL(app);
      ++num_apps;
    }

    return num_apps;
  }

  bool UninstallExternalWebAppByUrl(
      const GURL& app_url,
      ExternalInstallSource external_install_source) {
    bool result = false;
    base::RunLoop run_loop;
    finalizer().UninstallExternalWebAppByUrl(
        app_url,
        ConvertExternalInstallSourceToUninstallSource(external_install_source),
        base::BindLambdaForTesting([&](bool uninstalled) {
          result = uninstalled;
          run_loop.Quit();
        }));
    run_loop.Run();
    return result;
  }

  bool UninstallWebApp(const AppId& app_id) {
    bool result = false;
    base::RunLoop run_loop;
    finalizer().UninstallWebApp(
        app_id, webapps::WebappUninstallSource::kAppMenu,
        base::BindLambdaForTesting([&](bool uninstalled) {
          result = uninstalled;
          run_loop.Quit();
        }));
    run_loop.Run();
    return result;
  }

  void UseDefaultDataRetriever(const GURL& start_url) {
    install_manager().SetDataRetrieverFactoryForTesting(
        base::BindLambdaForTesting([start_url]() {
          auto data_retriever = std::make_unique<FakeDataRetriever>();
          data_retriever->BuildDefaultDataToRetrieve(start_url, start_url);
          return std::unique_ptr<WebAppDataRetriever>(
              std::move(data_retriever));
        }));
  }

  void DestroyManagers() {
    // The reverse order of creation:
    ui_manager_.reset();
    install_manager_.reset();
    install_finalizer_.reset();
    policy_manager_.reset();
    icon_manager_.reset();
    fake_registry_controller_.reset();
    externally_installed_app_prefs_.reset();

    test_url_loader_ = nullptr;
    file_utils_ = nullptr;
  }

  static std::string ParamInfoToString(
      testing::TestParamInfo<WebAppInstallManagerTest::ParamType> info) {
    switch (info.param) {
      case SyncParam::kWithSync:
        return "WithSync";
      case SyncParam::kWithoutSync:
        return "WithoutSync";
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  std::unique_ptr<FakeWebAppRegistryController> fake_registry_controller_;
  std::unique_ptr<WebAppIconManager> icon_manager_;
  std::unique_ptr<WebAppPolicyManager> policy_manager_;
  std::unique_ptr<WebAppInstallManager> install_manager_;
  std::unique_ptr<WebAppInstallFinalizer> install_finalizer_;
  std::unique_ptr<FakeWebAppUiManager> ui_manager_;
  std::unique_ptr<ExternallyInstalledWebAppPrefs>
      externally_installed_app_prefs_;

  // A weak ptr. The original is owned by install_manager_.
  TestWebAppUrlLoader* test_url_loader_ = nullptr;
  // Owned by icon_manager_:
  TestFileUtils* file_utils_ = nullptr;
};

using WebAppInstallManagerTest_SyncOnly = WebAppInstallManagerTest;

TEST_P(WebAppInstallManagerTest_SyncOnly,
       InstallWebAppsAfterSync_TwoConcurrentInstallsAreRunInOrder) {
  url_loader().AddPrepareForLoadResults({WebAppUrlLoader::Result::kUrlLoaded,
                                         WebAppUrlLoader::Result::kUrlLoaded});

  const GURL url1{"https://example.com/path"};
  const AppId app1_id = GenerateAppId(/*manifest_id=*/absl::nullopt, url1);

  const GURL url2{"https://example.org/path"};
  const AppId app2_id = GenerateAppId(/*manifest_id=*/absl::nullopt, url2);
  {
    std::unique_ptr<WebApp> app1 = CreateWebAppFromSyncAndPendingInstallation(
        url1, "Name1 from sync", DisplayMode::kStandalone, SK_ColorRED,
        /*is_locally_installed=*/false, /*scope=*/GURL(), /*icon_infos=*/{});

    std::unique_ptr<WebApp> app2 = CreateWebAppFromSyncAndPendingInstallation(
        url2, "Name2 from sync", DisplayMode::kBrowser, SK_ColorGREEN,
        /*is_locally_installed=*/true, /*scope=*/GURL(), /*icon_infos=*/{});

    Registry registry;
    registry.emplace(app1_id, std::move(app1));
    registry.emplace(app2_id, std::move(app2));

    InitRegistrarWithRegistry(registry);
  }

  // 1 InstallTask == 1 DataRetriever, their lifetime matches.
  base::flat_set<FakeDataRetriever*> task_data_retrievers;

  base::RunLoop app1_installed_run_loop;
  base::RunLoop app2_installed_run_loop;

  enum class Event {
    Task1_Queued,
    Task2_Queued,
    Task1_Started,
    Task1_Completed,
    App1_CallbackCalled,
    Task2_Started,
    Task2_Completed,
    App2_CallbackCalled,
  };

  std::vector<Event> event_order;

  int task_index = 0;

  install_manager().SetDataRetrieverFactoryForTesting(
      base::BindLambdaForTesting([&]() {
        auto data_retriever = std::make_unique<FakeDataRetriever>();
        task_index++;

        GURL start_url = task_index == 1 ? url1 : url2;
        data_retriever->BuildDefaultDataToRetrieve(start_url,
                                                   /*scope=*/start_url);

        FakeDataRetriever* data_retriever_ptr = data_retriever.get();
        task_data_retrievers.insert(data_retriever_ptr);

        event_order.push_back(task_index == 1 ? Event::Task1_Queued
                                              : Event::Task2_Queued);

        // Every InstallTask starts with WebAppDataRetriever::GetIcons step.
        data_retriever->SetGetIconsDelegate(base::BindLambdaForTesting(
            [&, task_index](content::WebContents* web_contents,
                            const std::vector<GURL>& icon_urls,
                            bool skip_page_favicons) {
              event_order.push_back(task_index == 1 ? Event::Task1_Started
                                                    : Event::Task2_Started);
              IconsMap icons_map;
              AddIconToIconsMap(GURL(kIconUrl), icon_size::k256, SK_ColorBLUE,
                                &icons_map);
              return icons_map;
            }));

        // Every InstallTask ends with WebAppDataRetriever destructor.
        data_retriever->SetDestructionCallback(
            base::BindLambdaForTesting([&task_data_retrievers, &event_order,
                                        data_retriever_ptr, task_index]() {
              event_order.push_back(task_index == 1 ? Event::Task1_Completed
                                                    : Event::Task2_Completed);
              task_data_retrievers.erase(data_retriever_ptr);
            }));

        return std::unique_ptr<WebAppDataRetriever>(std::move(data_retriever));
      }));

  EXPECT_FALSE(install_manager().has_web_contents_for_testing());

  WebApp* web_app1 =
      controller().mutable_registrar().GetAppByIdMutable(app1_id);
  WebApp* web_app2 =
      controller().mutable_registrar().GetAppByIdMutable(app2_id);
  ASSERT_TRUE(web_app1);
  ASSERT_TRUE(web_app2);

  url_loader().SetNextLoadUrlResult(url1, WebAppUrlLoader::Result::kUrlLoaded);
  url_loader().SetNextLoadUrlResult(url2, WebAppUrlLoader::Result::kUrlLoaded);

  // Enqueue a request to install the 1st app.
  install_manager().InstallWebAppsAfterSync(
      {web_app1}, base::BindLambdaForTesting([&](const AppId& installed_app_id,
                                                 InstallResultCode code) {
        EXPECT_EQ(InstallResultCode::kSuccessNewInstall, code);
        EXPECT_EQ(app1_id, installed_app_id);
        event_order.push_back(Event::App1_CallbackCalled);
        app1_installed_run_loop.Quit();
      }));

  EXPECT_TRUE(install_manager().has_web_contents_for_testing());
  EXPECT_EQ(0, GetNumFullyInstalledApps());
  EXPECT_EQ(1u, task_data_retrievers.size());

  // Immediately enqueue a request to install the 2nd app, WebContents is not
  // ready.
  install_manager().InstallWebAppsAfterSync(
      {web_app2}, base::BindLambdaForTesting([&](const AppId& installed_app_id,
                                                 InstallResultCode code) {
        EXPECT_EQ(InstallResultCode::kSuccessNewInstall, code);
        EXPECT_EQ(app2_id, installed_app_id);
        event_order.push_back(Event::App2_CallbackCalled);
        app2_installed_run_loop.Quit();
      }));

  EXPECT_TRUE(install_manager().has_web_contents_for_testing());
  EXPECT_EQ(2u, task_data_retrievers.size());
  EXPECT_EQ(0, GetNumFullyInstalledApps());

  // Wait for the 1st app installed.
  app1_installed_run_loop.Run();
  EXPECT_TRUE(install_manager().has_web_contents_for_testing());
  EXPECT_EQ(1u, task_data_retrievers.size());
  EXPECT_EQ(1, GetNumFullyInstalledApps());

  // Wait for the 2nd app installed.
  app2_installed_run_loop.Run();
  EXPECT_FALSE(install_manager().has_web_contents_for_testing());
  EXPECT_EQ(0u, task_data_retrievers.size());
  EXPECT_EQ(2, GetNumFullyInstalledApps());

  const std::vector<Event> expected_event_order{
      Event::Task1_Queued,    Event::Task2_Queued,        Event::Task1_Started,
      Event::Task1_Completed, Event::App1_CallbackCalled, Event::Task2_Started,
      Event::Task2_Completed, Event::App2_CallbackCalled,
  };

  EXPECT_EQ(expected_event_order, event_order);
}

TEST_P(WebAppInstallManagerTest_SyncOnly,
       InstallWebAppsAfterSync_InstallManagerDestroyed) {
  const GURL start_url{"https://example.com/path"};
  const AppId app_id = GenerateAppId(/*manifest_id=*/absl::nullopt, start_url);

  {
    std::unique_ptr<WebApp> app_in_sync_install =
        CreateWebAppFromSyncAndPendingInstallation(
            start_url, "Name from sync", DisplayMode::kStandalone, SK_ColorRED,
            /*is_locally_installed=*/true, /*scope=*/GURL(), /*icon_infos=*/{});

    InitRegistrarWithApp(std::move(app_in_sync_install));
  }

  base::RunLoop run_loop;

  install_manager().SetDataRetrieverFactoryForTesting(
      base::BindLambdaForTesting([&]() {
        auto data_retriever = std::make_unique<FakeDataRetriever>();
        data_retriever->BuildDefaultDataToRetrieve(start_url,
                                                   /*scope=*/start_url);

        // Every InstallTask starts with WebAppDataRetriever::GetIcons step.
        data_retriever->SetGetIconsDelegate(base::BindLambdaForTesting(
            [&](content::WebContents* web_contents,
                const std::vector<GURL>& icon_urls, bool skip_page_favicons) {
              run_loop.Quit();

              IconsMap icons_map;
              AddIconToIconsMap(GURL(kIconUrl), icon_size::k256, SK_ColorBLUE,
                                &icons_map);
              return icons_map;
            }));

        return std::unique_ptr<WebAppDataRetriever>(std::move(data_retriever));
      }));

  WebApp* web_app = controller().mutable_registrar().GetAppByIdMutable(app_id);

  url_loader().AddPrepareForLoadResults({WebAppUrlLoader::Result::kUrlLoaded});
  url_loader().SetNextLoadUrlResult(start_url,
                                    WebAppUrlLoader::Result::kUrlLoaded);

  bool callback_called = false;
  install_manager().InstallWebAppsAfterSync(
      {web_app}, base::BindLambdaForTesting(
                     [&](const AppId& installed_app_id,
                         InstallResultCode code) { callback_called = true; }));
  EXPECT_TRUE(install_manager().has_web_contents_for_testing());

  // Wait for the task to start.
  run_loop.Run();
  EXPECT_TRUE(install_manager().has_web_contents_for_testing());

  // Simulate Profile getting destroyed.
  DestroyManagers();

  EXPECT_FALSE(callback_called);
}

TEST_P(WebAppInstallManagerTest_SyncOnly, InstallWebAppsAfterSync_Success) {
  const std::string url_path{"https://example.com/path"};
  const GURL url{url_path};

  bool expect_locally_installed = AreAppsLocallyInstalledBySync();

  const std::unique_ptr<WebApp> expected_app =
      test::CreateWebApp(url, Source::kSync);
  expected_app->SetIsFromSyncAndPendingInstallation(false);
  expected_app->SetScope(url);
  expected_app->SetName("Name");
  expected_app->SetIsLocallyInstalled(expect_locally_installed);
  expected_app->SetDescription("Description");
  expected_app->SetThemeColor(SK_ColorCYAN);
  expected_app->SetDisplayMode(DisplayMode::kBrowser);
  expected_app->SetUserDisplayMode(DisplayMode::kStandalone);

  std::vector<apps::IconInfo> manifest_icons;
  std::vector<int> sizes;
  for (int size : SizesToGenerate()) {
    apps::IconInfo icon_info;
    icon_info.square_size_px = size;
    icon_info.url =
        GURL{url_path + "/icon" + base::NumberToString(size) + ".png"};
    manifest_icons.push_back(std::move(icon_info));
    sizes.push_back(size);
  }
  expected_app->SetManifestIcons(std::move(manifest_icons));
  expected_app->SetDownloadedIconSizes(IconPurpose::ANY, std::move(sizes));

  {
    WebApp::SyncFallbackData sync_fallback_data;
    sync_fallback_data.name = "Name";
    sync_fallback_data.theme_color = SK_ColorCYAN;
    sync_fallback_data.scope = url;
    sync_fallback_data.icon_infos = expected_app->manifest_icons();
    expected_app->SetSyncFallbackData(std::move(sync_fallback_data));
  }

  std::unique_ptr<const WebApp> app_in_sync_install =
      CreateWebAppFromSyncAndPendingInstallation(
          expected_app->start_url(), "Name from sync",
          expected_app->user_display_mode(), SK_ColorRED,
          expected_app->is_locally_installed(), expected_app->scope(),
          expected_app->manifest_icons());

  // Init using a copy.
  InitRegistrarWithApp(std::make_unique<WebApp>(*app_in_sync_install));

  WebApp* app = controller().mutable_registrar().GetAppByIdMutable(
      expected_app->app_id());

  url_loader().AddPrepareForLoadResults({WebAppUrlLoader::Result::kUrlLoaded});
  url_loader().SetNextLoadUrlResult(url, WebAppUrlLoader::Result::kUrlLoaded);

  install_manager().SetDataRetrieverFactoryForTesting(
      base::BindLambdaForTesting([&expected_app]() {
        return ConvertWebAppToDataRetriever(*expected_app);
      }));

  InstallResult result = InstallWebAppsAfterSync({app});
  EXPECT_EQ(InstallResultCode::kSuccessNewInstall, result.code);
  EXPECT_EQ(app->app_id(), result.app_id);

  EXPECT_EQ(1u, registrar().GetAppIds().size());
  EXPECT_EQ(app, registrar().GetAppById(expected_app->app_id()));

  EXPECT_NE(*app_in_sync_install, *app);
  EXPECT_NE(app_in_sync_install->sync_fallback_data(),
            app->sync_fallback_data());

  EXPECT_EQ(*expected_app, *app);
}

TEST_P(WebAppInstallManagerTest_SyncOnly, InstallWebAppsAfterSync_Fallback) {
  const GURL url{"https://example.com/path"};

  bool expect_locally_installed = AreAppsLocallyInstalledBySync();

  const std::unique_ptr<WebApp> expected_app =
      test::CreateWebApp(url, Source::kSync);
  expected_app->SetIsFromSyncAndPendingInstallation(false);
  expected_app->SetName("Name from sync");
  expected_app->SetScope(url);
  expected_app->SetDisplayMode(DisplayMode::kBrowser);
  expected_app->SetUserDisplayMode(DisplayMode::kBrowser);
  expected_app->SetIsLocallyInstalled(expect_locally_installed);
  expected_app->SetThemeColor(SK_ColorRED);
  // |scope| and |description| are empty here. |display_mode| is |kUndefined|.

  std::vector<apps::IconInfo> manifest_icons;
  std::vector<int> sizes;
  for (int size : SizesToGenerate()) {
    apps::IconInfo icon_info;
    icon_info.square_size_px = size;
    icon_info.url =
        GURL{url.spec() + "/icon" + base::NumberToString(size) + ".png"};
    manifest_icons.push_back(std::move(icon_info));
    sizes.push_back(size);
  }
  expected_app->SetManifestIcons(std::move(manifest_icons));
  expected_app->SetDownloadedIconSizes(IconPurpose::ANY, std::move(sizes));
  expected_app->SetIsGeneratedIcon(true);

  {
    WebApp::SyncFallbackData sync_fallback_data;
    sync_fallback_data.name = "Name from sync";
    sync_fallback_data.theme_color = SK_ColorRED;
    sync_fallback_data.scope = expected_app->scope();
    sync_fallback_data.icon_infos = expected_app->manifest_icons();
    expected_app->SetSyncFallbackData(std::move(sync_fallback_data));
  }

  std::unique_ptr<const WebApp> app_in_sync_install =
      CreateWebAppFromSyncAndPendingInstallation(
          expected_app->start_url(), expected_app->name(),
          expected_app->user_display_mode(),
          expected_app->theme_color().value(),
          expected_app->is_locally_installed(), expected_app->scope(),
          expected_app->manifest_icons());

  // Init using a copy.
  InitRegistrarWithApp(std::make_unique<WebApp>(*app_in_sync_install));

  WebApp* app = controller().mutable_registrar().GetAppByIdMutable(
      expected_app->app_id());

  // Simulate if the web app publisher's website is down.
  url_loader().SetNextLoadUrlResult(
      url, WebAppUrlLoader::Result::kFailedPageTookTooLong);
  // about:blank will be loaded twice, one for the initial attempt and one for
  // the fallback attempt.
  url_loader().AddPrepareForLoadResults({WebAppUrlLoader::Result::kUrlLoaded,
                                         WebAppUrlLoader::Result::kUrlLoaded});

  install_manager().SetDataRetrieverFactoryForTesting(
      base::BindLambdaForTesting([]() {
        // The data retrieval stage must not be reached if url fails to load.
        return CreateEmptyDataRetriever();
      }));

  InstallResult result = InstallWebAppsAfterSync({app});
  EXPECT_EQ(InstallResultCode::kSuccessNewInstall, result.code);
  EXPECT_EQ(app->app_id(), result.app_id);

  EXPECT_EQ(1u, registrar().GetAppIds().size());
  EXPECT_EQ(app, registrar().GetAppById(expected_app->app_id()));

  EXPECT_NE(*app_in_sync_install, *app);
  EXPECT_EQ(app_in_sync_install->sync_fallback_data(),
            app->sync_fallback_data());

  EXPECT_EQ(*expected_app, *app);
}

TEST_P(WebAppInstallManagerTest_SyncOnly,
       UninstallFromSyncAfterRegistryUpdate) {
  std::unique_ptr<WebApp> app =
      test::CreateWebApp(GURL("https://example.com/path"), Source::kSync);
  app->SetUserDisplayMode(DisplayMode::kStandalone);

  const AppId app_id = app->app_id();
  InitRegistrarWithApp(std::move(app));

  file_utils().SetNextDeleteFileRecursivelyResult(true);

  enum Event {
    kUninstallFromSyncBeforeRegistryUpdate,
    kObserver_OnWebAppWillBeUninstalled,
    kObserver_OnWebAppUninstalled,
    kUninstallFromSyncAfterRegistryUpdate_Callback
  };
  std::vector<Event> event_order;

  WebAppTestRegistryObserverAdapter observer(&registrar());
  observer.SetWebAppWillBeUninstalledDelegate(
      base::BindLambdaForTesting([&](const AppId& uninstalled_app_id) {
        EXPECT_EQ(uninstalled_app_id, app_id);
        event_order.push_back(Event::kObserver_OnWebAppWillBeUninstalled);
      }));
  observer.SetWebAppUninstalledDelegate(
      base::BindLambdaForTesting([&](const AppId& uninstalled_app_id) {
        EXPECT_EQ(uninstalled_app_id, app_id);
        event_order.push_back(Event::kObserver_OnWebAppUninstalled);
      }));

  controller().SetUninstallFromSyncBeforeRegistryUpdateDelegate(
      base::BindLambdaForTesting([&](std::vector<AppId> apps_to_uninstall) {
        ASSERT_FALSE(apps_to_uninstall.empty());
        EXPECT_EQ(apps_to_uninstall[0], app_id);
        event_order.push_back(Event::kUninstallFromSyncBeforeRegistryUpdate);
        install_manager().UninstallFromSyncBeforeRegistryUpdate(
            std::move(apps_to_uninstall));
      }));

  base::RunLoop run_loop;
  controller().SetUninstallFromSyncAfterRegistryUpdateDelegate(
      base::BindLambdaForTesting(
          [&](std::vector<std::unique_ptr<WebApp>> apps_unregistered,
              SyncInstallDelegate::RepeatingUninstallCallback callback) {
            install_manager().UninstallFromSyncAfterRegistryUpdate(
                std::move(apps_unregistered),
                base::BindLambdaForTesting([&](const AppId& uninstalled_app_id,
                                               bool uninstalled) {
                  EXPECT_EQ(uninstalled_app_id, app_id);
                  EXPECT_TRUE(uninstalled);
                  event_order.push_back(
                      Event::kUninstallFromSyncAfterRegistryUpdate_Callback);
                  run_loop.Quit();
                }));
          }));

  // The sync server sends a change to delete the app.
  sync_bridge_test_utils::DeleteApps(controller().sync_bridge(), {app_id});
  run_loop.Run();

  const std::vector<Event> expected_event_order{
      Event::kUninstallFromSyncBeforeRegistryUpdate,
      Event::kObserver_OnWebAppWillBeUninstalled,
      Event::kObserver_OnWebAppUninstalled,
      Event::kUninstallFromSyncAfterRegistryUpdate_Callback};
  EXPECT_EQ(expected_event_order, event_order);
}

TEST_P(WebAppInstallManagerTest_SyncOnly,
       PolicyAndUser_UninstallExternalWebApp) {
  std::unique_ptr<WebApp> policy_and_user_app =
      test::CreateWebApp(GURL("https://example.com/path"), Source::kSync);
  policy_and_user_app->AddSource(Source::kPolicy);
  policy_and_user_app->SetUserDisplayMode(DisplayMode::kStandalone);

  const AppId app_id = policy_and_user_app->app_id();
  const GURL external_app_url("https://example.com/path/policy");

  externally_installed_app_prefs().Insert(
      external_app_url, app_id, ExternalInstallSource::kExternalPolicy);
  InitRegistrarWithApp(std::move(policy_and_user_app));

  EXPECT_FALSE(finalizer().WasPreinstalledWebAppUninstalled(app_id));

  bool observer_uninstall_called = false;
  WebAppTestRegistryObserverAdapter observer(&registrar());
  observer.SetWebAppUninstalledDelegate(
      base::BindLambdaForTesting([&](const AppId& uninstalled_app_id) {
        observer_uninstall_called = true;
      }));

  // Unknown url fails.
  EXPECT_FALSE(UninstallExternalWebAppByUrl(
      GURL("https://example.org/"), ExternalInstallSource::kExternalPolicy));

  // Uninstall policy app first.
  EXPECT_TRUE(UninstallExternalWebAppByUrl(
      external_app_url, ExternalInstallSource::kExternalPolicy));

  EXPECT_TRUE(registrar().GetAppById(app_id));
  EXPECT_FALSE(observer_uninstall_called);
  EXPECT_FALSE(finalizer().WasPreinstalledWebAppUninstalled(app_id));
  EXPECT_TRUE(finalizer().CanUserUninstallWebApp(app_id));
}

TEST_P(WebAppInstallManagerTest_SyncOnly, DefaultAndUser_UninstallWebApp) {
  std::unique_ptr<WebApp> default_and_user_app =
      test::CreateWebApp(GURL("https://example.com/path"), Source::kSync);
  default_and_user_app->AddSource(Source::kDefault);
  default_and_user_app->SetUserDisplayMode(DisplayMode::kStandalone);

  const AppId app_id = default_and_user_app->app_id();
  const GURL external_app_url("https://example.com/path/default");

  externally_installed_app_prefs().Insert(
      external_app_url, app_id, ExternalInstallSource::kExternalDefault);
  InitRegistrarWithApp(std::move(default_and_user_app));

  EXPECT_TRUE(finalizer().CanUserUninstallWebApp(app_id));
  EXPECT_FALSE(finalizer().WasPreinstalledWebAppUninstalled(app_id));

  WebAppTestRegistryObserverAdapter observer(&registrar());

  bool observer_uninstalled_called = false;

  observer.SetWebAppUninstalledDelegate(
      base::BindLambdaForTesting([&](const AppId& uninstalled_app_id) {
        EXPECT_EQ(app_id, uninstalled_app_id);
        observer_uninstalled_called = true;
      }));

  file_utils().SetNextDeleteFileRecursivelyResult(true);

  EXPECT_TRUE(UninstallWebApp(app_id));

  EXPECT_FALSE(registrar().GetAppById(app_id));
  EXPECT_TRUE(observer_uninstalled_called);
  EXPECT_FALSE(finalizer().CanUserUninstallWebApp(app_id));
  EXPECT_TRUE(finalizer().WasPreinstalledWebAppUninstalled(app_id));
}

TEST_P(WebAppInstallManagerTest, InstallWebAppFromInfo) {
  InitEmptyRegistrar();

  const GURL url("https://example.com/path");
  const AppId expected_app_id =
      GenerateAppId(/*manifest_id=*/absl::nullopt, url);

  auto server_web_app_info = std::make_unique<WebApplicationInfo>();
  server_web_app_info->start_url = url;
  server_web_app_info->scope = url;
  server_web_app_info->title = u"Test web app";

  const webapps::WebappInstallSource install_source =
      AreSystemWebAppsSupported()
          ? webapps::WebappInstallSource::SYSTEM_DEFAULT
          : webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON;

  InstallResult result = InstallWebAppFromInfo(
      std::move(server_web_app_info),
      /*overwrite_existing_manifest_fields=*/false, install_source);
  EXPECT_EQ(InstallResultCode::kSuccessNewInstall, result.code);
  EXPECT_EQ(expected_app_id, result.app_id);

  const WebApp* web_app = registrar().GetAppById(expected_app_id);
  ASSERT_TRUE(web_app);

  std::map<SquareSizePx, SkBitmap> icon_bitmaps =
      ReadIcons(expected_app_id, IconPurpose::ANY,
                web_app->downloaded_icon_sizes(IconPurpose::ANY));

  // Make sure that icons have been generated for all sub sizes.
  EXPECT_TRUE(ContainsOneIconOfEachSize(icon_bitmaps));
}

TEST_P(WebAppInstallManagerTest, TaskQueueWebContentsReadyRace) {
  InitEmptyRegistrar();

  std::unique_ptr<WebAppInstallTask> task_a = CreateDummyTask();
  WebAppInstallTask* task_a_ptr = task_a.get();
  std::unique_ptr<WebAppInstallTask> task_b = CreateDummyTask();
  std::unique_ptr<WebAppInstallTask> task_c = CreateDummyTask();

  // Enqueue task A and await it to be started.
  base::RunLoop run_loop_a_start;
  url_loader().SetPrepareForLoadResultLoaded();
  install_manager().EnsureWebContentsCreated();
  install_manager().EnqueueTask(std::move(task_a),
                                run_loop_a_start.QuitClosure());
  run_loop_a_start.Run();

  // Enqueue task B before A has finished.
  bool task_b_started = false;
  install_manager().EnqueueTask(
      std::move(task_b),
      base::BindLambdaForTesting([&]() { task_b_started = true; }));

  // Finish task A.
  url_loader().SetPrepareForLoadResultLoaded();
  install_manager().OnQueuedTaskCompleted(
      task_a_ptr, base::DoNothing(), AppId(),
      InstallResultCode::kSuccessNewInstall);

  // Task B needs to wait for WebContents to return ready.
  EXPECT_FALSE(task_b_started);

  // Enqueue task C before B has started.
  bool task_c_started = false;
  install_manager().EnqueueTask(
      std::move(task_c),
      base::BindLambdaForTesting([&]() { task_c_started = true; }));

  // Task C should not start before B has started.
  EXPECT_FALSE(task_b_started);
  EXPECT_FALSE(task_c_started);
}

TEST_P(WebAppInstallManagerTest_SyncOnly,
       InstallWebAppFromManifestWithFallback_OverwriteIsLocallyInstalled) {
  const GURL start_url{"https://example.com/path"};
  const AppId app_id = GenerateAppId(/*manifest_id=*/absl::nullopt, start_url);

  {
    std::unique_ptr<WebApp> app_in_sync_install =
        CreateWebAppFromSyncAndPendingInstallation(
            start_url, "Name from sync",
            /*user_display_mode=*/DisplayMode::kStandalone, SK_ColorRED,
            /*is_locally_installed=*/false, /*scope=*/GURL(),
            /*icon_infos=*/{});

    InitRegistrarWithApp(std::move(app_in_sync_install));
  }

  EXPECT_FALSE(registrar().IsLocallyInstalled(app_id));
  EXPECT_EQ(DisplayMode::kBrowser,
            registrar().GetAppEffectiveDisplayMode(app_id));

  // DefaultDataRetriever returns DisplayMode::kStandalone app's display mode.
  UseDefaultDataRetriever(start_url);

  InstallResult result = InstallWebAppFromManifestWithFallback();
  EXPECT_EQ(InstallResultCode::kSuccessNewInstall, result.code);
  EXPECT_EQ(app_id, result.app_id);

  EXPECT_TRUE(registrar().IsInstalled(app_id));
  EXPECT_TRUE(registrar().IsLocallyInstalled(app_id));
  // InstallWebAppFromManifestWithFallback sets user_display_mode to kBrowser
  // because TestAcceptDialogCallback doesn't set open_as_window to true.
  EXPECT_EQ(DisplayMode::kBrowser,
            registrar().GetAppEffectiveDisplayMode(app_id));
}

TEST_P(WebAppInstallManagerTest_SyncOnly,
       InstallWebAppFromWebAppStoreThenInstallFromSync) {
  InitEmptyRegistrar();

  const GURL start_url("https://example.com/path");
  const AppId app_id = GenerateAppId(/*manifest_id=*/absl::nullopt, start_url);

  // Reproduces `ApkWebAppInstaller` install parameters.
  auto apk_web_application_info = std::make_unique<WebApplicationInfo>();
  apk_web_application_info->start_url = start_url;
  apk_web_application_info->scope = GURL("https://example.com/apk_scope");
  apk_web_application_info->title = u"Name from APK";
  apk_web_application_info->theme_color = SK_ColorWHITE;
  apk_web_application_info->display_mode = DisplayMode::kStandalone;
  apk_web_application_info->user_display_mode = DisplayMode::kStandalone;
  AddGeneratedIcon(&apk_web_application_info->icon_bitmaps.any, icon_size::k128,
                   SK_ColorYELLOW);

  InstallResult result =
      InstallWebAppFromInfo(std::move(apk_web_application_info),
                            /*overwrite_existing_manifest_fields=*/false,
                            webapps::WebappInstallSource::ARC);

  EXPECT_EQ(InstallResultCode::kSuccessNewInstall, result.code);
  EXPECT_EQ(app_id, result.app_id);

  const WebApp* web_app = registrar().GetAppById(app_id);
  ASSERT_TRUE(web_app);

  EXPECT_TRUE(web_app->IsWebAppStoreInstalledApp());
  EXPECT_FALSE(web_app->IsSynced());
  EXPECT_FALSE(web_app->is_from_sync_and_pending_installation());

  ASSERT_TRUE(web_app->theme_color().has_value());
  EXPECT_EQ(SK_ColorWHITE, web_app->theme_color().value());
  EXPECT_EQ("Name from APK", web_app->name());
  EXPECT_EQ("https://example.com/apk_scope", web_app->scope().spec());

  ASSERT_TRUE(web_app->sync_fallback_data().theme_color.has_value());
  EXPECT_EQ(SK_ColorWHITE, web_app->sync_fallback_data().theme_color.value());
  EXPECT_EQ("Name from APK", web_app->sync_fallback_data().name);
  EXPECT_EQ("https://example.com/apk_scope",
            web_app->sync_fallback_data().scope.spec());

  EXPECT_EQ(DisplayMode::kStandalone, web_app->display_mode());
  EXPECT_EQ(DisplayMode::kStandalone, web_app->user_display_mode());

  EXPECT_TRUE(web_app->manifest_icons().empty());
  EXPECT_TRUE(web_app->sync_fallback_data().icon_infos.empty());

  EXPECT_EQ(SK_ColorYELLOW, IconManagerReadAppIconPixel(icon_manager(), app_id,
                                                        icon_size::k128));

  // Simulates the same web app arriving from sync.
  {
    auto synced_specifics_data = std::make_unique<WebApp>(app_id);
    synced_specifics_data->SetStartUrl(start_url);

    synced_specifics_data->AddSource(Source::kSync);
    synced_specifics_data->SetUserDisplayMode(DisplayMode::kBrowser);
    synced_specifics_data->SetName("Name From Sync");

    WebApp::SyncFallbackData sync_fallback_data;
    sync_fallback_data.name = "Name From Sync";
    sync_fallback_data.theme_color = SK_ColorMAGENTA;
    sync_fallback_data.scope = GURL("https://example.com/sync_scope");

    apps::IconInfo apps_icon_info = CreateIconInfo(
        /*icon_base_url=*/start_url, IconPurpose::MONOCHROME, icon_size::k64);
    sync_fallback_data.icon_infos.push_back(std::move(apps_icon_info));

    synced_specifics_data->SetSyncFallbackData(std::move(sync_fallback_data));

    // `SyncInstallDelegate::InstallWebAppsAfterSync()` must not be called.
    controller().SetInstallWebAppsAfterSyncDelegate(base::BindLambdaForTesting(
        [&](std::vector<WebApp*> apps_to_install,
            FakeWebAppRegistryController::RepeatingInstallCallback callback) {
          ADD_FAILURE();
        }));

    std::vector<std::unique_ptr<WebApp>> add_synced_apps_data;
    add_synced_apps_data.push_back(std::move(synced_specifics_data));
    sync_bridge_test_utils::AddApps(controller().sync_bridge(),
                                    add_synced_apps_data);
  }

  EXPECT_EQ(web_app, registrar().GetAppById(app_id));

  EXPECT_TRUE(web_app->IsWebAppStoreInstalledApp());
  EXPECT_TRUE(web_app->IsSynced());
  EXPECT_FALSE(web_app->is_from_sync_and_pending_installation());

  EXPECT_EQ(DisplayMode::kStandalone, web_app->display_mode());
  EXPECT_EQ(DisplayMode::kBrowser, web_app->user_display_mode());

  ASSERT_TRUE(web_app->theme_color().has_value());
  EXPECT_EQ(SK_ColorWHITE, web_app->theme_color().value());
  EXPECT_EQ("Name from APK", web_app->name());
  EXPECT_EQ("https://example.com/apk_scope", web_app->scope().spec());

  ASSERT_TRUE(web_app->sync_fallback_data().theme_color.has_value());
  EXPECT_EQ(SK_ColorMAGENTA, web_app->sync_fallback_data().theme_color.value());
  EXPECT_EQ("Name From Sync", web_app->sync_fallback_data().name);
  EXPECT_EQ("https://example.com/sync_scope",
            web_app->sync_fallback_data().scope.spec());

  EXPECT_TRUE(web_app->manifest_icons().empty());
  ASSERT_EQ(1u, web_app->sync_fallback_data().icon_infos.size());

  const apps::IconInfo& app_icon_info =
      web_app->sync_fallback_data().icon_infos[0];
  EXPECT_EQ(apps::IconInfo::Purpose::kMonochrome, app_icon_info.purpose);
  EXPECT_EQ(icon_size::k64, app_icon_info.square_size_px);
  EXPECT_EQ("https://example.com/icon-64.png", app_icon_info.url.spec());

  EXPECT_EQ(SK_ColorYELLOW, IconManagerReadAppIconPixel(icon_manager(), app_id,
                                                        icon_size::k128));
}

INSTANTIATE_TEST_SUITE_P(All,
                         WebAppInstallManagerTest,
#if BUILDFLAG(IS_CHROMEOS_ASH)
                         ::testing::Values(SyncParam::kWithoutSync,
                                           SyncParam::kWithSync),
#else
                         ::testing::Values(SyncParam::kWithSync),
#endif
                         WebAppInstallManagerTest::ParamInfoToString);

INSTANTIATE_TEST_SUITE_P(All,
                         WebAppInstallManagerTest_SyncOnly,
                         ::testing::Values(SyncParam::kWithSync),
                         WebAppInstallManagerTest::ParamInfoToString);

}  // namespace web_app
