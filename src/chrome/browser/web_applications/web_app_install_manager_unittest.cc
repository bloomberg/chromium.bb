// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_install_manager.h"

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/flat_set.h"
#include "base/run_loop.h"
#include "base/strings/nullable_string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/web_applications/components/externally_installed_web_app_prefs.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/components/web_app_icon_generator.h"
#include "chrome/browser/web_applications/components/web_app_utils.h"
#include "chrome/browser/web_applications/test/test_app_shortcut_manager.h"
#include "chrome/browser/web_applications/test/test_data_retriever.h"
#include "chrome/browser/web_applications/test/test_file_handler_manager.h"
#include "chrome/browser/web_applications/test/test_file_utils.h"
#include "chrome/browser/web_applications/test/test_web_app_database_factory.h"
#include "chrome/browser/web_applications/test/test_web_app_registry_controller.h"
#include "chrome/browser/web_applications/test/test_web_app_ui_manager.h"
#include "chrome/browser/web_applications/test/test_web_app_url_loader.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_install_observer.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom-shared.h"
#include "third_party/skia/include/core/SkColor.h"
#include "url/gurl.h"

namespace web_app {

namespace {

// TODO(https://crbug.com/1042727): Fix test GURL scoping and remove this getter
// function.
GURL IconUrl() {
  return GURL("https://example.com/app.ico");
}

base::NullableString16 ToNullableUTF16(const std::string& str) {
  return base::NullableString16(base::UTF8ToUTF16(str), false);
}

std::unique_ptr<WebApplicationInfo> ConvertWebAppToRendererWebApplicationInfo(
    const WebApp& app) {
  auto web_application_info = std::make_unique<WebApplicationInfo>();
  // Most fields are expected to be populated by a manifest data in a subsequent
  // override install process data flow. TODO(loyso): Make it more robust.
  web_application_info->description = base::UTF8ToUTF16(app.description());
  // |open_as_window| is a user's display mode value and it is typically
  // populated by a UI dialog in production code. We set it here for testing
  // purposes.
  web_application_info->open_as_window =
      app.user_display_mode() == DisplayMode::kStandalone;
  return web_application_info;
}

std::vector<blink::Manifest::ImageResource> ConvertWebAppIconsToImageResources(
    const WebApp& app) {
  std::vector<blink::Manifest::ImageResource> icons;
  for (const WebApplicationIconInfo& icon_info : app.icon_infos()) {
    blink::Manifest::ImageResource icon;
    icon.src = icon_info.url;
    icon.purpose.push_back(blink::Manifest::ImageResource::Purpose::ANY);
    icon.sizes.push_back(
        gfx::Size(icon_info.square_size_px, icon_info.square_size_px));
    icons.push_back(std::move(icon));
  }
  return icons;
}

std::unique_ptr<blink::Manifest> ConvertWebAppToManifest(const WebApp& app) {
  auto manifest = std::make_unique<blink::Manifest>();
  manifest->start_url = app.launch_url();
  manifest->scope = app.launch_url();
  manifest->short_name = ToNullableUTF16("Short Name to be overriden.");
  manifest->name = ToNullableUTF16(app.name());
  manifest->theme_color = app.theme_color();
  manifest->display = app.display_mode();
  manifest->icons = ConvertWebAppIconsToImageResources(app);
  return manifest;
}

IconsMap ConvertWebAppIconsToIconsMap(const WebApp& app) {
  IconsMap icons_map;
  for (const WebApplicationIconInfo& icon_info : app.icon_infos()) {
    icons_map[icon_info.url] = {
        CreateSquareIcon(icon_info.square_size_px, SK_ColorBLACK)};
  }
  return icons_map;
}

std::unique_ptr<WebAppDataRetriever> ConvertWebAppToDataRetriever(
    const WebApp& app) {
  auto data_retriever = std::make_unique<TestDataRetriever>();

  data_retriever->SetRendererWebApplicationInfo(
      ConvertWebAppToRendererWebApplicationInfo(app));
  data_retriever->SetManifest(ConvertWebAppToManifest(app),
                              /*is_installable=*/true);
  data_retriever->SetIcons(ConvertWebAppIconsToIconsMap(app));

  return std::unique_ptr<WebAppDataRetriever>(std::move(data_retriever));
}

std::unique_ptr<WebAppDataRetriever> CreateEmptyDataRetriever() {
  auto data_retriever = std::make_unique<TestDataRetriever>();
  return std::unique_ptr<WebAppDataRetriever>(std::move(data_retriever));
}

}  // namespace

class WebAppInstallManagerTest : public WebAppTest {
 public:
  void SetUp() override {
    WebAppTest::SetUp();

    externally_installed_app_prefs_ =
        std::make_unique<ExternallyInstalledWebAppPrefs>(profile()->GetPrefs());

    test_registry_controller_ =
        std::make_unique<TestWebAppRegistryController>();
    test_registry_controller_->SetUp(profile());

    auto file_utils = std::make_unique<TestFileUtils>();
    file_utils_ = file_utils.get();

    icon_manager_ = std::make_unique<WebAppIconManager>(profile(), registrar(),
                                                        std::move(file_utils));

    install_finalizer_ = std::make_unique<WebAppInstallFinalizer>(
        profile(), icon_manager_.get());

    shortcut_manager_ = std::make_unique<TestAppShortcutManager>(profile());
    file_handler_manager_ = std::make_unique<TestFileHandlerManager>(profile());

    install_manager_ = std::make_unique<WebAppInstallManager>(profile());
    install_manager_->SetSubsystems(&registrar(), shortcut_manager_.get(),
                                    file_handler_manager_.get(),
                                    install_finalizer_.get());

    auto test_url_loader = std::make_unique<TestWebAppUrlLoader>();

    test_url_loader->SetAboutBlankResultLoaded();

    test_url_loader_ = test_url_loader.get();
    install_manager_->SetUrlLoaderForTesting(std::move(test_url_loader));

    ui_manager_ = std::make_unique<TestWebAppUiManager>();

    install_finalizer_->SetSubsystems(
        &registrar(), ui_manager_.get(),
        &test_registry_controller_->sync_bridge());
  }

  void TearDown() override {
    DestroyManagers();
    WebAppTest::TearDown();
  }

  WebAppRegistrar& registrar() { return controller().registrar(); }
  WebAppInstallManager& install_manager() { return *install_manager_; }
  TestAppShortcutManager& shortcut_manager() { return *shortcut_manager_; }
  TestFileHandlerManager& file_handler_manager() {
    return *file_handler_manager_;
  }
  WebAppInstallFinalizer& finalizer() { return *install_finalizer_; }
  WebAppIconManager& icon_manager() { return *icon_manager_; }
  TestWebAppUrlLoader& url_loader() { return *test_url_loader_; }
  TestFileUtils& file_utils() {
    DCHECK(file_utils_);
    return *file_utils_;
  }
  TestWebAppRegistryController& controller() {
    return *test_registry_controller_;
  }
  ExternallyInstalledWebAppPrefs& externally_installed_app_prefs() {
    return *externally_installed_app_prefs_;
  }

  std::unique_ptr<WebApplicationInfo> CreateWebAppInfo(const GURL& url) {
    auto web_app_info = std::make_unique<WebApplicationInfo>();
    web_app_info->app_url = url;
    WebApplicationIconInfo icon_info;
    icon_info.url = IconUrl();
    icon_info.square_size_px = icon_size::k256;
    web_app_info->icon_infos.push_back(std::move(icon_info));
    return web_app_info;
  }

  std::unique_ptr<WebApp> CreateWebApp(const GURL& launch_url,
                                       Source::Type source,
                                       DisplayMode user_display_mode) {
    const AppId app_id = GenerateAppIdFromURL(launch_url);

    auto web_app = std::make_unique<WebApp>(app_id);
    web_app->SetLaunchUrl(launch_url);

    web_app->AddSource(source);
    web_app->SetUserDisplayMode(user_display_mode);
    return web_app;
  }

  std::unique_ptr<WebApp> CreateWebAppInSyncInstall(
      const GURL& launch_url,
      const std::string& app_name,
      DisplayMode user_display_mode,
      SkColor theme_color,
      bool locally_installed,
      const std::vector<WebApplicationIconInfo>& icon_infos) {
    auto web_app = CreateWebApp(launch_url, Source::kSync, user_display_mode);
    web_app->SetIsInSyncInstall(true);
    web_app->SetIsLocallyInstalled(locally_installed);
    web_app->SetIconInfos(icon_infos);

    WebApp::SyncData sync_data;
    sync_data.name = app_name;
    sync_data.theme_color = theme_color;
    web_app->SetSyncData(std::move(sync_data));
    return web_app;
  }

  void InitEmptyRegistrar() { controller().Init(); }

  std::set<AppId> InitRegistrarWithRegistry(const Registry& registry) {
    std::set<AppId> app_ids;
    for (auto& kv : registry)
      app_ids.insert(kv.second->app_id());

    controller().database_factory().WriteRegistry(registry);
    controller().Init();

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

  InstallResult InstallBookmarkAppFromSync(
      const AppId& bookmark_app_id,
      std::unique_ptr<WebApplicationInfo> server_web_application_info) {
    InstallResult result;
    base::RunLoop run_loop;
    install_manager().InstallBookmarkAppFromSync(
        bookmark_app_id, std::move(server_web_application_info),
        base::BindLambdaForTesting(
            [&](const AppId& installed_app_id, InstallResultCode code) {
              result.app_id = installed_app_id;
              result.code = code;
              run_loop.Quit();
            }));
    run_loop.Run();
    return result;
  }

  AppId InstallBookmarkAppFromSync(const GURL& url,
                                   bool server_open_as_window) {
    const AppId bookmark_app_id = GenerateAppIdFromURL(url);

    auto server_web_application_info = std::make_unique<WebApplicationInfo>();
    server_web_application_info->app_url = url;
    server_web_application_info->open_as_window = server_open_as_window;
    InstallResult result = InstallBookmarkAppFromSync(
        bookmark_app_id, std::move(server_web_application_info));

    EXPECT_EQ(InstallResultCode::kSuccessNewInstall, result.code);
    return result.app_id;
  }

  std::map<SquareSizePx, SkBitmap> ReadIcons(
      const AppId& app_id,
      std::vector<SquareSizePx> sizes_px) {
    std::map<SquareSizePx, SkBitmap> result;
    base::RunLoop run_loop;
    icon_manager().ReadIcons(
        app_id, sizes_px,
        base::BindLambdaForTesting(
            [&](std::map<SquareSizePx, SkBitmap> icon_bitmaps) {
              result = std::move(icon_bitmaps);
              run_loop.Quit();
            }));
    run_loop.Run();
    return result;
  }

  InstallResult FinalizeInstall(
      const WebApplicationInfo& web_app_info,
      const InstallFinalizer::FinalizeOptions& options) {
    InstallResult result;
    base::RunLoop run_loop;
    finalizer().FinalizeInstall(
        web_app_info, options,
        base::BindLambdaForTesting(
            [&](const AppId& app_id, InstallResultCode code) {
              result.app_id = app_id;
              result.code = code;
              run_loop.Quit();
            }));
    run_loop.Run();
    return result;
  }

  int GetNumFullyInstalledApps() const {
    int num_apps = 0;
    for (const WebApp& app : test_registry_controller_->registrar().AllApps()) {
      if (!app.is_in_sync_install())
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
        app_url, external_install_source,
        base::BindLambdaForTesting([&](bool uninstalled) {
          result = uninstalled;
          run_loop.Quit();
        }));
    run_loop.Run();
    return result;
  }

  bool UninstallWebAppFromSyncByUser(const AppId& app_id) {
    bool result = false;
    base::RunLoop run_loop;
    finalizer().UninstallWebAppFromSyncByUser(
        app_id, base::BindLambdaForTesting([&](bool uninstalled) {
          result = uninstalled;
          run_loop.Quit();
        }));
    run_loop.Run();
    return result;
  }

  bool UninstallExternalAppByUser(const AppId& app_id) {
    bool result = false;
    base::RunLoop run_loop;
    finalizer().UninstallExternalAppByUser(
        app_id, base::BindLambdaForTesting([&](bool uninstalled) {
          result = uninstalled;
          run_loop.Quit();
        }));
    run_loop.Run();
    return result;
  }

  void UseDefaultDataRetriever(const GURL& launch_url) {
    install_manager().SetDataRetrieverFactoryForTesting(
        base::BindLambdaForTesting([launch_url]() {
          auto data_retriever = std::make_unique<TestDataRetriever>();
          data_retriever->BuildDefaultDataToRetrieve(launch_url, launch_url);
          return std::unique_ptr<WebAppDataRetriever>(
              std::move(data_retriever));
        }));
  }

  void DestroyManagers() {
    // The reverse order of creation:
    ui_manager_.reset();
    install_manager_.reset();
    shortcut_manager_.reset();
    install_finalizer_.reset();
    icon_manager_.reset();
    test_registry_controller_.reset();
    externally_installed_app_prefs_.reset();

    test_url_loader_ = nullptr;
    file_utils_ = nullptr;
  }

 private:
  std::unique_ptr<TestWebAppRegistryController> test_registry_controller_;
  std::unique_ptr<WebAppIconManager> icon_manager_;

  std::unique_ptr<TestAppShortcutManager> shortcut_manager_;
  std::unique_ptr<TestFileHandlerManager> file_handler_manager_;
  std::unique_ptr<WebAppInstallManager> install_manager_;
  std::unique_ptr<WebAppInstallFinalizer> install_finalizer_;
  std::unique_ptr<TestWebAppUiManager> ui_manager_;
  std::unique_ptr<ExternallyInstalledWebAppPrefs>
      externally_installed_app_prefs_;

  // A weak ptr. The original is owned by install_manager_.
  TestWebAppUrlLoader* test_url_loader_ = nullptr;
  // Owned by icon_manager_:
  TestFileUtils* file_utils_ = nullptr;
};

TEST_F(WebAppInstallManagerTest,
       InstallWebAppsAfterSync_TwoConcurrentInstallsAreRunInOrder) {
  const GURL url1{"https://example.com/path"};
  const AppId app1_id = GenerateAppIdFromURL(url1);

  const GURL url2{"https://example.org/path"};
  const AppId app2_id = GenerateAppIdFromURL(url2);
  {
    std::unique_ptr<WebApp> app1 = CreateWebAppInSyncInstall(
        url1, "Name1 from sync", DisplayMode::kStandalone, SK_ColorRED,
        /*is_locally_installed=*/false, /*icon_infos=*/{});

    std::unique_ptr<WebApp> app2 = CreateWebAppInSyncInstall(
        url2, "Name2 from sync", DisplayMode::kBrowser, SK_ColorGREEN,
        /*is_locally_installed=*/true, /*icon_infos=*/{});

    Registry registry;
    registry.emplace(app1_id, std::move(app1));
    registry.emplace(app2_id, std::move(app2));

    InitRegistrarWithRegistry(registry);
  }

  // 1 InstallTask == 1 DataRetriever, their lifetime matches.
  base::flat_set<TestDataRetriever*> task_data_retrievers;

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
        auto data_retriever = std::make_unique<TestDataRetriever>();
        task_index++;

        GURL launch_url = task_index == 1 ? url1 : url2;
        data_retriever->BuildDefaultDataToRetrieve(launch_url,
                                                   /*scope=*/launch_url);

        TestDataRetriever* data_retriever_ptr = data_retriever.get();
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
              AddIconToIconsMap(IconUrl(), icon_size::k256, SK_ColorBLUE,
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

TEST_F(WebAppInstallManagerTest,
       InstallWebAppsAfterSync_InstallManagerDestroyed) {
  const GURL launch_url{"https://example.com/path"};
  const AppId app_id = GenerateAppIdFromURL(launch_url);

  {
    std::unique_ptr<WebApp> app_in_sync_install = CreateWebAppInSyncInstall(
        launch_url, "Name from sync", DisplayMode::kStandalone, SK_ColorRED,
        /*is_locally_installed=*/true, /*icon_infos=*/{});

    InitRegistrarWithApp(std::move(app_in_sync_install));
  }

  base::RunLoop run_loop;

  install_manager().SetDataRetrieverFactoryForTesting(
      base::BindLambdaForTesting([&]() {
        auto data_retriever = std::make_unique<TestDataRetriever>();
        data_retriever->BuildDefaultDataToRetrieve(launch_url,
                                                   /*scope=*/launch_url);

        // Every InstallTask starts with WebAppDataRetriever::GetIcons step.
        data_retriever->SetGetIconsDelegate(base::BindLambdaForTesting(
            [&](content::WebContents* web_contents,
                const std::vector<GURL>& icon_urls, bool skip_page_favicons) {
              run_loop.Quit();

              IconsMap icons_map;
              AddIconToIconsMap(IconUrl(), icon_size::k256, SK_ColorBLUE,
                                &icons_map);
              return icons_map;
            }));

        return std::unique_ptr<WebAppDataRetriever>(std::move(data_retriever));
      }));

  WebApp* web_app = controller().mutable_registrar().GetAppByIdMutable(app_id);

  url_loader().SetNextLoadUrlResult(launch_url,
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

TEST_F(WebAppInstallManagerTest, InstallWebAppsAfterSync_Success) {
  const std::string url_path{"https://example.com/path"};
  const GURL url{url_path};

  const std::unique_ptr<WebApp> expected_app =
      CreateWebApp(url, Source::kSync,
                   /*user_display_mode=*/DisplayMode::kStandalone);
  expected_app->SetIsInSyncInstall(false);
  expected_app->SetScope(url);
  expected_app->SetName("Name");
  expected_app->SetIsLocallyInstalled(false);
  expected_app->SetDescription("Description");
  expected_app->SetThemeColor(SK_ColorCYAN);
  expected_app->SetDisplayMode(DisplayMode::kBrowser);
  {
    WebApp::SyncData sync_data;
    sync_data.name = "Name";
    sync_data.theme_color = SK_ColorCYAN;
    expected_app->SetSyncData(std::move(sync_data));
  }

  std::vector<WebApplicationIconInfo> icon_infos;
  std::vector<int> sizes;
  for (int size : SizesToGenerate()) {
    WebApplicationIconInfo icon_info;
    icon_info.square_size_px = size;
    icon_info.url =
        GURL{url_path + "/icon" + base::NumberToString(size) + ".png"};
    icon_infos.push_back(std::move(icon_info));
    sizes.push_back(size);
  }
  expected_app->SetIconInfos(std::move(icon_infos));
  expected_app->SetDownloadedIconSizes(std::move(sizes));

  std::unique_ptr<const WebApp> app_in_sync_install = CreateWebAppInSyncInstall(
      expected_app->launch_url(), "Name from sync",
      expected_app->user_display_mode(), SK_ColorRED,
      expected_app->is_locally_installed(), expected_app->icon_infos());

  // Init using a copy.
  InitRegistrarWithApp(std::make_unique<WebApp>(*app_in_sync_install));

  WebApp* app = controller().mutable_registrar().GetAppByIdMutable(
      expected_app->app_id());

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
  EXPECT_NE(app_in_sync_install->sync_data(), app->sync_data());

  EXPECT_EQ(*expected_app, *app);
}

TEST_F(WebAppInstallManagerTest, InstallWebAppsAfterSync_Fallback) {
  const GURL url{"https://example.com/path"};

  const std::unique_ptr<WebApp> expected_app =
      CreateWebApp(url, Source::kSync,
                   /*user_display_mode=*/DisplayMode::kBrowser);
  expected_app->SetIsInSyncInstall(false);
  expected_app->SetName("Name from sync");
  expected_app->SetIsLocallyInstalled(false);
  expected_app->SetThemeColor(SK_ColorRED);
  // |scope| and |description| are empty here. |display_mode| is |kUndefined|.
  {
    WebApp::SyncData sync_data;
    sync_data.name = "Name from sync";
    sync_data.theme_color = SK_ColorRED;
    expected_app->SetSyncData(std::move(sync_data));
  }

  std::vector<WebApplicationIconInfo> icon_infos;
  std::vector<int> sizes;
  for (int size : SizesToGenerate()) {
    WebApplicationIconInfo icon_info;
    icon_info.square_size_px = size;
    icon_info.url =
        GURL{url.spec() + "/icon" + base::NumberToString(size) + ".png"};
    icon_infos.push_back(std::move(icon_info));
    sizes.push_back(size);
  }
  expected_app->SetIconInfos(std::move(icon_infos));
  expected_app->SetDownloadedIconSizes(std::move(sizes));

  std::unique_ptr<const WebApp> app_in_sync_install = CreateWebAppInSyncInstall(
      expected_app->launch_url(), expected_app->name(),
      expected_app->user_display_mode(), expected_app->theme_color().value(),
      expected_app->is_locally_installed(), expected_app->icon_infos());

  // Init using a copy.
  InitRegistrarWithApp(std::make_unique<WebApp>(*app_in_sync_install));

  WebApp* app = controller().mutable_registrar().GetAppByIdMutable(
      expected_app->app_id());

  // Simulate if the web app publisher's website is down.
  url_loader().SetNextLoadUrlResult(
      url, WebAppUrlLoader::Result::kFailedPageTookTooLong);

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
  EXPECT_EQ(app_in_sync_install->sync_data(), app->sync_data());

  EXPECT_EQ(*expected_app, *app);
}

TEST_F(WebAppInstallManagerTest, UninstallWebAppsAfterSync) {
  std::unique_ptr<WebApp> app =
      CreateWebApp(GURL("https://example.com/path"), Source::kSync,
                   /*user_display_mode=*/DisplayMode::kStandalone);

  const AppId app_id = app->app_id();
  InitRegistrarWithApp(std::move(app));

  file_utils().SetNextDeleteFileRecursivelyResult(true);

  enum Event {
    kObserver_OnWebAppUninstalled,
    kUninstallWebAppsAfterSync_Callback
  };
  std::vector<Event> event_order;

  WebAppInstallObserver observer(&registrar());
  observer.SetWebAppUninstalledDelegate(
      base::BindLambdaForTesting([&](const AppId& uninstalled_app_id) {
        EXPECT_EQ(uninstalled_app_id, app_id);
        event_order.push_back(Event::kObserver_OnWebAppUninstalled);
      }));

  base::RunLoop run_loop;

  controller().SetUninstallWebAppsAfterSyncDelegate(base::BindLambdaForTesting(
      [&](std::vector<std::unique_ptr<WebApp>> apps_unregistered,
          SyncInstallDelegate::RepeatingUninstallCallback callback) {
        install_manager().UninstallWebAppsAfterSync(
            std::move(apps_unregistered),
            base::BindLambdaForTesting([&](const AppId& uninstalled_app_id,
                                           bool uninstalled) {
              EXPECT_EQ(uninstalled_app_id, app_id);
              EXPECT_TRUE(uninstalled);
              event_order.push_back(Event::kUninstallWebAppsAfterSync_Callback);
              run_loop.Quit();
            }));
      }));

  // The sync server sends a change to delete the app.
  controller().ApplySyncChanges_DeleteApps({app_id});
  run_loop.Run();

  const std::vector<Event> expected_event_order{
      Event::kObserver_OnWebAppUninstalled,
      Event::kUninstallWebAppsAfterSync_Callback};
  EXPECT_EQ(expected_event_order, event_order);
}

TEST_F(WebAppInstallManagerTest, PolicyAndUser_UninstallExternalWebApp) {
  std::unique_ptr<WebApp> policy_and_user_app =
      CreateWebApp(GURL("https://example.com/path"), Source::kSync,
                   /*user_display_mode=*/DisplayMode::kStandalone);
  policy_and_user_app->AddSource(Source::kPolicy);

  const AppId app_id = policy_and_user_app->app_id();
  const GURL external_app_url("https://example.com/path/policy");

  externally_installed_app_prefs().Insert(
      external_app_url, app_id, ExternalInstallSource::kExternalPolicy);
  InitRegistrarWithApp(std::move(policy_and_user_app));

  EXPECT_TRUE(finalizer().CanUserUninstallFromSync(app_id));
  EXPECT_FALSE(finalizer().WasExternalAppUninstalledByUser(app_id));

  bool observer_uninstall_called = false;
  WebAppInstallObserver observer(&registrar());
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
  EXPECT_TRUE(finalizer().CanUserUninstallFromSync(app_id));
  EXPECT_FALSE(finalizer().WasExternalAppUninstalledByUser(app_id));
  EXPECT_TRUE(finalizer().CanUserUninstallExternalApp(app_id));

  // Uninstall user app last.
  file_utils().SetNextDeleteFileRecursivelyResult(true);

  EXPECT_TRUE(UninstallWebAppFromSyncByUser(app_id));

  EXPECT_FALSE(registrar().GetAppById(app_id));
  EXPECT_TRUE(observer_uninstall_called);
  EXPECT_FALSE(finalizer().CanUserUninstallFromSync(app_id));
  EXPECT_FALSE(finalizer().WasExternalAppUninstalledByUser(app_id));
  EXPECT_FALSE(finalizer().CanUserUninstallExternalApp(app_id));
}

TEST_F(WebAppInstallManagerTest, PolicyAndUser_UninstallWebAppFromSyncByUser) {
  std::unique_ptr<WebApp> policy_and_user_app =
      CreateWebApp(GURL("https://example.com/path"), Source::kSync,
                   /*user_display_mode=*/DisplayMode::kStandalone);
  policy_and_user_app->AddSource(Source::kPolicy);

  const AppId app_id = policy_and_user_app->app_id();
  const GURL external_app_url("https://example.com/path/policy");

  externally_installed_app_prefs().Insert(
      external_app_url, app_id, ExternalInstallSource::kExternalPolicy);
  InitRegistrarWithApp(std::move(policy_and_user_app));

  EXPECT_TRUE(finalizer().CanUserUninstallFromSync(app_id));
  EXPECT_FALSE(finalizer().CanUserUninstallExternalApp(app_id));

  bool observer_uninstall_called = false;
  WebAppInstallObserver observer(&registrar());
  observer.SetWebAppUninstalledDelegate(
      base::BindLambdaForTesting([&](const AppId& uninstalled_app_id) {
        observer_uninstall_called = true;
      }));

  // Uninstall user app first.
  EXPECT_TRUE(UninstallWebAppFromSyncByUser(app_id));

  EXPECT_TRUE(registrar().GetAppById(app_id));
  EXPECT_FALSE(observer_uninstall_called);
  EXPECT_FALSE(finalizer().CanUserUninstallFromSync(app_id));
  EXPECT_FALSE(finalizer().WasExternalAppUninstalledByUser(app_id));
  EXPECT_FALSE(finalizer().CanUserUninstallExternalApp(app_id));

  // Uninstall policy app last.
  file_utils().SetNextDeleteFileRecursivelyResult(true);

  EXPECT_TRUE(UninstallExternalWebAppByUrl(
      external_app_url, ExternalInstallSource::kExternalPolicy));
  EXPECT_FALSE(registrar().GetAppById(app_id));
  EXPECT_TRUE(observer_uninstall_called);
  EXPECT_FALSE(finalizer().WasExternalAppUninstalledByUser(app_id));
  EXPECT_FALSE(finalizer().CanUserUninstallExternalApp(app_id));
}

TEST_F(WebAppInstallManagerTest, DefaultAndUser_UninstallExternalAppByUser) {
  std::unique_ptr<WebApp> default_and_user_app =
      CreateWebApp(GURL("https://example.com/path"), Source::kSync,
                   /*user_display_mode=*/DisplayMode::kStandalone);
  default_and_user_app->AddSource(Source::kDefault);

  const AppId app_id = default_and_user_app->app_id();
  const GURL external_app_url("https://example.com/path/default");

  externally_installed_app_prefs().Insert(
      external_app_url, app_id, ExternalInstallSource::kExternalDefault);
  InitRegistrarWithApp(std::move(default_and_user_app));

  EXPECT_TRUE(finalizer().CanUserUninstallExternalApp(app_id));
  EXPECT_FALSE(finalizer().WasExternalAppUninstalledByUser(app_id));

  WebAppInstallObserver observer(&registrar());

  bool observer_uninstalled_called = false;

  observer.SetWebAppUninstalledDelegate(
      base::BindLambdaForTesting([&](const AppId& uninstalled_app_id) {
        EXPECT_EQ(app_id, uninstalled_app_id);
        observer_uninstalled_called = true;
      }));

  file_utils().SetNextDeleteFileRecursivelyResult(true);

  EXPECT_TRUE(UninstallExternalAppByUser(app_id));

  EXPECT_FALSE(registrar().GetAppById(app_id));
  EXPECT_TRUE(observer_uninstalled_called);
  EXPECT_FALSE(finalizer().CanUserUninstallExternalApp(app_id));
  EXPECT_TRUE(finalizer().WasExternalAppUninstalledByUser(app_id));
}

TEST_F(WebAppInstallManagerTest, InstallBookmarkAppFromSync_LoadSuccess) {
  InitEmptyRegistrar();

  const auto url1 = GURL("https://example.com/");
  const auto url2 = GURL("https://example.org/");

  url_loader().SetNextLoadUrlResult(url1, WebAppUrlLoader::Result::kUrlLoaded);
  install_manager().SetDataRetrieverFactoryForTesting(
      base::BindLambdaForTesting([&]() {
        auto data_retriever = std::make_unique<TestDataRetriever>();
        data_retriever->BuildDefaultDataToRetrieve(url1, url1);
        auto web_site_application_info = std::make_unique<WebApplicationInfo>();
        web_site_application_info->open_as_window = false;
        web_site_application_info->display_mode = DisplayMode::kBrowser;
        data_retriever->SetRendererWebApplicationInfo(
            std::move(web_site_application_info));
        return std::unique_ptr<WebAppDataRetriever>(std::move(data_retriever));
      }));
  const AppId app_id1 =
      InstallBookmarkAppFromSync(url1, /*server_open_as_window=*/true);

  url_loader().SetNextLoadUrlResult(url2, WebAppUrlLoader::Result::kUrlLoaded);
  url_loader().SetAboutBlankResultLoaded();
  install_manager().SetDataRetrieverFactoryForTesting(
      base::BindLambdaForTesting([&]() {
        auto data_retriever = std::make_unique<TestDataRetriever>();
        data_retriever->BuildDefaultDataToRetrieve(url2, url2);
        auto web_site_application_info = std::make_unique<WebApplicationInfo>();
        web_site_application_info->open_as_window = false;
        web_site_application_info->display_mode = DisplayMode::kStandalone;
        data_retriever->SetRendererWebApplicationInfo(
            std::move(web_site_application_info));
        return std::unique_ptr<WebAppDataRetriever>(std::move(data_retriever));
      }));
  const AppId app_id2 =
      InstallBookmarkAppFromSync(url2, /*server_open_as_window=*/false);

#if defined(OS_CHROMEOS)
  EXPECT_TRUE(registrar().GetAppById(app_id1)->is_locally_installed());
#else  // !defined(OS_CHROMEOS)
  EXPECT_FALSE(registrar().GetAppById(app_id1)->is_locally_installed());
#endif

  EXPECT_EQ(registrar().GetAppDisplayMode(app_id1), DisplayMode::kBrowser);
  EXPECT_EQ(registrar().GetAppDisplayMode(app_id2), DisplayMode::kStandalone);

  EXPECT_EQ(registrar().GetAppUserDisplayMode(app_id1),
            DisplayMode::kStandalone);
  EXPECT_EQ(registrar().GetAppUserDisplayMode(app_id2), DisplayMode::kBrowser);
}

TEST_F(WebAppInstallManagerTest, InstallBookmarkAppFromSync_LoadFailed) {
  InitEmptyRegistrar();

  const auto url1 = GURL("https://example.com/");
  const auto url2 = GURL("https://example.org/");
  // Induce a load failure:
  url_loader().SetNextLoadUrlResult(
      url1, WebAppUrlLoader::Result::kRedirectedUrlLoaded);
  url_loader().SetNextLoadUrlResult(
      url2, WebAppUrlLoader::Result::kRedirectedUrlLoaded);

  auto app_id1 =
      InstallBookmarkAppFromSync(url1, /*server_open_as_window=*/false);

  url_loader().SetAboutBlankResultLoaded();
  auto app_id2 =
      InstallBookmarkAppFromSync(url2, /*server_open_as_window=*/true);

#if defined(OS_CHROMEOS)
  EXPECT_TRUE(registrar().GetAppById(app_id1)->is_locally_installed());
#else  // !defined(OS_CHROMEOS)
  EXPECT_FALSE(registrar().GetAppById(app_id1)->is_locally_installed());
#endif

  EXPECT_EQ(registrar().GetAppDisplayMode(app_id1), DisplayMode::kBrowser);
  EXPECT_EQ(registrar().GetAppDisplayMode(app_id2), DisplayMode::kBrowser);

  EXPECT_EQ(registrar().GetAppUserDisplayMode(app_id1), DisplayMode::kBrowser);
  EXPECT_EQ(registrar().GetAppUserDisplayMode(app_id2),
            DisplayMode::kStandalone);
}

TEST_F(WebAppInstallManagerTest, InstallBookmarkAppFromSync_TwoIcons_Success) {
  InitEmptyRegistrar();

  const GURL url{"https://example.com/path"};
  const GURL icon1_url{"https://example.com/path/icon1.png"};
  const GURL icon2_url{"https://example.com/path/icon2.png"};
  url_loader().SetNextLoadUrlResult(url, WebAppUrlLoader::Result::kUrlLoaded);

  const AppId app_id = GenerateAppIdFromURL(url);

  auto server_web_app_info = std::make_unique<WebApplicationInfo>();
  server_web_app_info->app_url = url;
  {
    WebApplicationIconInfo server_icon1_info;
    server_icon1_info.url = icon1_url;
    server_icon1_info.square_size_px = icon_size::k128;
    server_web_app_info->icon_infos.push_back(std::move(server_icon1_info));

    WebApplicationIconInfo server_icon2_info;
    server_icon2_info.url = icon2_url;
    server_icon2_info.square_size_px = icon_size::k256;
    server_web_app_info->icon_infos.push_back(std::move(server_icon2_info));
  }

  install_manager().SetDataRetrieverFactoryForTesting(
      base::BindLambdaForTesting([&]() {
        auto data_retriever = std::make_unique<TestDataRetriever>();
        data_retriever->BuildDefaultDataToRetrieve(url, url);
        // Set the website manifest to be a copy of WebApplicationInfo from
        // sync, as if they are the same.
        std::unique_ptr<WebApplicationInfo> site_web_app_info =
            std::make_unique<WebApplicationInfo>(*server_web_app_info);
        data_retriever->SetRendererWebApplicationInfo(
            std::move(site_web_app_info));

        IconsMap site_icons_map;
        AddIconToIconsMap(icon1_url, icon_size::k128, SK_ColorBLUE,
                          &site_icons_map);
        AddIconToIconsMap(icon2_url, icon_size::k256, SK_ColorRED,
                          &site_icons_map);

        data_retriever->SetIcons(std::move(site_icons_map));
        return std::unique_ptr<WebAppDataRetriever>(std::move(data_retriever));
      }));

  InstallResult result = InstallBookmarkAppFromSync(
      app_id, std::make_unique<WebApplicationInfo>(*server_web_app_info));
  EXPECT_EQ(InstallResultCode::kSuccessNewInstall, result.code);
  EXPECT_EQ(app_id, result.app_id);

  const WebApp* web_app = registrar().GetAppById(app_id);

  EXPECT_EQ(2U, web_app->icon_infos().size());
  EXPECT_EQ(SizesToGenerate().size(), web_app->downloaded_icon_sizes().size());

  EXPECT_EQ(icon1_url, web_app->icon_infos().at(0).url);
  EXPECT_EQ(icon2_url, web_app->icon_infos().at(1).url);

  // Read icons from disk to check pixel contents.
  std::map<SquareSizePx, SkBitmap> icon_bitmaps =
      ReadIcons(app_id, {icon_size::k128, icon_size::k256});
  EXPECT_EQ(2u, icon_bitmaps.size());

  const auto& icon1 = icon_bitmaps[icon_size::k128];
  EXPECT_FALSE(icon1.drawsNothing());
  EXPECT_EQ(SK_ColorBLUE, icon1.getColor(0, 0));

  const auto& icon2 = icon_bitmaps[icon_size::k256];
  EXPECT_FALSE(icon2.drawsNothing());
  EXPECT_EQ(SK_ColorRED, icon2.getColor(0, 0));
}

TEST_F(WebAppInstallManagerTest, InstallBookmarkAppFromSync_TwoIcons_Fallback) {
  InitEmptyRegistrar();

  const GURL url{"https://example.com/path"};
  const GURL icon1_url{"https://example.com/path/icon1.png"};
  const GURL icon2_url{"https://example.com/path/icon2.png"};
  // Induce a load failure:
  url_loader().SetNextLoadUrlResult(
      url, WebAppUrlLoader::Result::kRedirectedUrlLoaded);
  install_manager().SetDataRetrieverFactoryForTesting(
      base::BindLambdaForTesting([&]() {
        return std::unique_ptr<WebAppDataRetriever>(
            std::make_unique<TestDataRetriever>());
      }));

  const AppId app_id = GenerateAppIdFromURL(url);

  auto server_web_app_info = std::make_unique<WebApplicationInfo>();
  server_web_app_info->app_url = url;
  server_web_app_info->generated_icon_color = SK_ColorBLUE;
  {
    WebApplicationIconInfo server_icon1_info;
    server_icon1_info.url = icon1_url;
    server_icon1_info.square_size_px = icon_size::k128;
    server_web_app_info->icon_infos.push_back(std::move(server_icon1_info));

    WebApplicationIconInfo server_icon2_info;
    server_icon2_info.url = icon2_url;
    server_icon2_info.square_size_px = icon_size::k256;
    server_web_app_info->icon_infos.push_back(std::move(server_icon2_info));
  }

  InstallResult result =
      InstallBookmarkAppFromSync(app_id, std::move(server_web_app_info));
  EXPECT_EQ(InstallResultCode::kSuccessNewInstall, result.code);
  EXPECT_EQ(app_id, result.app_id);

  const WebApp* web_app = registrar().GetAppById(app_id);

  EXPECT_EQ(2U, web_app->icon_infos().size());
  EXPECT_EQ(SizesToGenerate().size(), web_app->downloaded_icon_sizes().size());

  EXPECT_EQ(icon1_url, web_app->icon_infos().at(0).url);
  EXPECT_EQ(icon2_url, web_app->icon_infos().at(1).url);

  // Read icons from disk. All icons get the E letter drawn into a rounded
  // blue background.
  std::map<SquareSizePx, SkBitmap> icon_bitmaps =
      ReadIcons(app_id, {icon_size::k128, icon_size::k256});
  EXPECT_EQ(2u, icon_bitmaps.size());

  const auto& icon1 = icon_bitmaps[icon_size::k128];
  EXPECT_FALSE(icon1.drawsNothing());

  const auto& icon2 = icon_bitmaps[icon_size::k256];
  EXPECT_FALSE(icon2.drawsNothing());
}

TEST_F(WebAppInstallManagerTest, InstallBookmarkAppFromSync_NoIcons) {
  InitEmptyRegistrar();

  const GURL url{"https://example.com/path"};
  // Induce a load failure:
  url_loader().SetNextLoadUrlResult(
      url, WebAppUrlLoader::Result::kRedirectedUrlLoaded);
  UseDefaultDataRetriever(url);
  const AppId app_id = GenerateAppIdFromURL(url);

  auto web_app_info = std::make_unique<WebApplicationInfo>();
  web_app_info->app_url = url;
  // All icons will get the E letter drawn into a rounded yellow background.
  web_app_info->generated_icon_color = SK_ColorYELLOW;

  InstallResult result =
      InstallBookmarkAppFromSync(app_id, std::move(web_app_info));
  EXPECT_EQ(InstallResultCode::kSuccessNewInstall, result.code);
  EXPECT_EQ(app_id, result.app_id);

  const WebApp* web_app = registrar().GetAppById(app_id);

  std::map<SquareSizePx, SkBitmap> icon_bitmaps =
      ReadIcons(app_id, web_app->downloaded_icon_sizes());

  // Make sure that icons have been generated for all sub sizes.
  EXPECT_TRUE(ContainsOneIconOfEachSize(icon_bitmaps));

  for (const std::pair<const SquareSizePx, SkBitmap>& icon : icon_bitmaps)
    EXPECT_FALSE(icon.second.drawsNothing());
}

TEST_F(WebAppInstallManagerTest, InstallBookmarkAppFromSync_ExpectAppIdFailed) {
  InitEmptyRegistrar();

  const GURL old_url{"https://example.com/path"};
  url_loader().SetNextLoadUrlResult(old_url,
                                    WebAppUrlLoader::Result::kUrlLoaded);

  // The web site has changed app url:
  UseDefaultDataRetriever(GURL{"https://example.org"});

  const AppId expected_app_id = GenerateAppIdFromURL(old_url);

  auto server_web_app_info = std::make_unique<WebApplicationInfo>();
  server_web_app_info->app_url = old_url;

  // WebAppInstallTask finishes with kExpectedAppIdCheckFailed but
  // WebAppInstallManager falls back to web application info, received from the
  // server.
  InstallResult result = InstallBookmarkAppFromSync(
      expected_app_id, std::move(server_web_app_info));
  EXPECT_EQ(InstallResultCode::kSuccessNewInstall, result.code);
  EXPECT_EQ(expected_app_id, result.app_id);

  const WebApp* web_app = registrar().GetAppById(expected_app_id);
  ASSERT_TRUE(web_app);

  std::map<SquareSizePx, SkBitmap> icon_bitmaps =
      ReadIcons(expected_app_id, web_app->downloaded_icon_sizes());

  // Make sure that icons have been generated for all sub sizes.
  EXPECT_TRUE(ContainsOneIconOfEachSize(icon_bitmaps));
}

}  // namespace web_app
