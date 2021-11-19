// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/app_service/web_apps_publisher_host.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <vector>

#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/values.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_factory.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/notifications/notification_common.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "chrome/browser/web_applications/externally_installed_web_app_prefs.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_application_info.h"
#include "chrome/common/webui_url_constants.h"
#include "chromeos/crosapi/mojom/app_service.mojom.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/display/types/display_constants.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/message_center/public/cpp/notifier_id.h"
#include "url/gurl.h"

using apps::IconEffects;

namespace web_app {

class MockAppPublisher : public crosapi::mojom::AppPublisher {
 public:
  MockAppPublisher() { run_loop_ = std::make_unique<base::RunLoop>(); }
  ~MockAppPublisher() override = default;

  void Wait() {
    run_loop_->Run();
    run_loop_ = std::make_unique<base::RunLoop>();
  }

  const std::vector<apps::mojom::AppPtr>& get_deltas() const {
    return app_deltas_;
  }

  const std::vector<apps::mojom::CapabilityAccessPtr>&
  get_capability_access_deltas() const {
    return capability_access_deltas_;
  }

 private:
  // crosapi::mojom::AppPublisher:
  void OnApps(std::vector<apps::mojom::AppPtr> deltas) override {
    app_deltas_.insert(app_deltas_.end(),
                       std::make_move_iterator(deltas.begin()),
                       std::make_move_iterator(deltas.end()));
    run_loop_->Quit();
  }

  void RegisterAppController(
      mojo::PendingRemote<crosapi::mojom::AppController> controller) override {
    NOTIMPLEMENTED();
  }

  void OnCapabilityAccesses(
      std::vector<apps::mojom::CapabilityAccessPtr> deltas) override {
    capability_access_deltas_.insert(capability_access_deltas_.end(),
                                     std::make_move_iterator(deltas.begin()),
                                     std::make_move_iterator(deltas.end()));
    run_loop_->Quit();
  }

  std::vector<apps::mojom::AppPtr> app_deltas_;
  std::vector<apps::mojom::CapabilityAccessPtr> capability_access_deltas_;
  std::unique_ptr<base::RunLoop> run_loop_;
};

class WebAppsPublisherHostBrowserTest : public WebAppControllerBrowserTest {
 public:
  WebAppsPublisherHostBrowserTest() = default;
  ~WebAppsPublisherHostBrowserTest() override = default;
};

IN_PROC_BROWSER_TEST_F(WebAppsPublisherHostBrowserTest, PublishApps) {
  ASSERT_TRUE(embedded_test_server()->Start());
  InstallWebAppFromManifest(
      browser(), embedded_test_server()->GetURL("/web_apps/basic.html"));
  InstallWebAppFromManifest(browser(), embedded_test_server()->GetURL(
                                           "/web_share_target/charts.html"));

  MockAppPublisher mock_app_publisher;
  WebAppsPublisherHost web_apps_publisher_host(profile());
  web_apps_publisher_host.SetPublisherForTesting(&mock_app_publisher);
  web_apps_publisher_host.Init();
  mock_app_publisher.Wait();
  EXPECT_EQ(mock_app_publisher.get_deltas().size(), 2U);

  AppId app_id = InstallWebAppFromManifest(
      browser(),
      embedded_test_server()->GetURL("/banners/manifest_test_page.html"));
  mock_app_publisher.Wait();

  // OnWebAppInstalled() and OnWebAppInstalledWithOsHooks() lead to updates.
  EXPECT_EQ(mock_app_publisher.get_deltas().size(), 4U);
  EXPECT_EQ(mock_app_publisher.get_deltas().back()->app_id, app_id);
  EXPECT_EQ(mock_app_publisher.get_deltas().back()->readiness,
            apps::mojom::Readiness::kReady);
  EXPECT_EQ(mock_app_publisher.get_deltas().back()->icon_key->icon_effects,
            IconEffects::kRoundCorners | IconEffects::kCrOsStandardIcon);

  {
    base::RunLoop run_loop;
    UninstallWebAppWithCallback(
        profile(), app_id,
        base::BindLambdaForTesting([&run_loop](bool uninstalled) {
          EXPECT_TRUE(uninstalled);
          run_loop.Quit();
        }));
    run_loop.Run();
    mock_app_publisher.Wait();
    EXPECT_EQ(mock_app_publisher.get_deltas().back()->app_id, app_id);
    EXPECT_EQ(mock_app_publisher.get_deltas().back()->readiness,
              apps::mojom::Readiness::kUninstalledByUser);
  }
}

IN_PROC_BROWSER_TEST_F(WebAppsPublisherHostBrowserTest, LaunchTime) {
  ASSERT_TRUE(embedded_test_server()->Start());
  AppId app_id = InstallWebAppFromManifest(
      browser(), embedded_test_server()->GetURL("/web_apps/basic.html"));

  MockAppPublisher mock_app_publisher;
  WebAppsPublisherHost web_apps_publisher_host(profile());
  web_apps_publisher_host.SetPublisherForTesting(&mock_app_publisher);
  web_apps_publisher_host.Init();
  mock_app_publisher.Wait();
  ASSERT_TRUE(
      mock_app_publisher.get_deltas().back()->last_launch_time.has_value());
  const base::Time last_launch_time =
      *mock_app_publisher.get_deltas().back()->last_launch_time;

  LaunchWebAppBrowser(app_id);
  mock_app_publisher.Wait();
  EXPECT_EQ(mock_app_publisher.get_deltas().back()->app_id, app_id);
  ASSERT_TRUE(
      mock_app_publisher.get_deltas().back()->last_launch_time.has_value());
  EXPECT_GT(*mock_app_publisher.get_deltas().back()->last_launch_time,
            last_launch_time);
}

IN_PROC_BROWSER_TEST_F(WebAppsPublisherHostBrowserTest, ManifestUpdate) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL app_url =
      embedded_test_server()->GetURL("app.site.com", "/simple.html");

  MockAppPublisher mock_app_publisher;
  WebAppsPublisherHost web_apps_publisher_host(profile());
  web_apps_publisher_host.SetPublisherForTesting(&mock_app_publisher);
  web_apps_publisher_host.Init();

  AppId app_id;
  {
    const std::u16string original_description = u"Original Web App";
    auto web_app_info = std::make_unique<WebApplicationInfo>();
    web_app_info->start_url = app_url;
    web_app_info->scope = app_url;
    web_app_info->title = original_description;
    web_app_info->description = original_description;
    app_id = InstallWebApp(std::move(web_app_info));

    mock_app_publisher.Wait();
    EXPECT_EQ(*mock_app_publisher.get_deltas().back()->description,
              base::UTF16ToUTF8(original_description));
  }

  {
    const std::u16string updated_description = u"Updated Web App";
    auto web_app_info = std::make_unique<WebApplicationInfo>();
    web_app_info->start_url = app_url;
    web_app_info->scope = app_url;
    web_app_info->title = updated_description;
    web_app_info->description = updated_description;

    base::RunLoop run_loop;
    provider().install_finalizer().FinalizeUpdate(
        *web_app_info,
        base::BindLambdaForTesting(
            [&run_loop](const AppId& app_id, InstallResultCode code) {
              run_loop.Quit();
            }));

    run_loop.Run();
    mock_app_publisher.Wait();
    EXPECT_EQ(*mock_app_publisher.get_deltas().back()->description,
              base::UTF16ToUTF8(updated_description));
  }
}

IN_PROC_BROWSER_TEST_F(WebAppsPublisherHostBrowserTest, LocallyInstalledState) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL app_url =
      embedded_test_server()->GetURL("app.site.com", "/simple.html");

  AppId app_id;
  {
    const std::u16string description = u"Web App";
    auto web_app_info = std::make_unique<WebApplicationInfo>();
    web_app_info->start_url = app_url;
    web_app_info->scope = app_url;
    web_app_info->title = description;
    web_app_info->description = description;
    app_id = InstallWebApp(std::move(web_app_info));

    provider().sync_bridge().SetAppIsLocallyInstalled(
        app_id,
        /*is_locally_installed=*/false);
  }

  MockAppPublisher mock_app_publisher;
  WebAppsPublisherHost web_apps_publisher_host(profile());
  web_apps_publisher_host.SetPublisherForTesting(&mock_app_publisher);
  web_apps_publisher_host.Init();
  mock_app_publisher.Wait();
  EXPECT_EQ(mock_app_publisher.get_deltas().back()->icon_key->icon_effects,
            static_cast<IconEffects>(IconEffects::kRoundCorners |
                                     IconEffects::kBlocked |
                                     IconEffects::kCrOsStandardMask));

  provider().sync_bridge().SetAppIsLocallyInstalled(
      app_id,
      /*is_locally_installed=*/true);
  mock_app_publisher.Wait();
  EXPECT_EQ(mock_app_publisher.get_deltas().back()->icon_key->icon_effects,
            IconEffects::kRoundCorners | IconEffects::kCrOsStandardMask);
}

IN_PROC_BROWSER_TEST_F(WebAppsPublisherHostBrowserTest, PolicyId) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL app_url = embedded_test_server()->GetURL("/web_apps/basic.html");
  const GURL install_url =
      embedded_test_server()->GetURL("/web_apps/get_manifest.html?basic.json");
  AppId app_id = InstallWebAppFromPage(browser(), install_url);
  MockAppPublisher mock_app_publisher;
  WebAppsPublisherHost web_apps_publisher_host(profile());
  web_apps_publisher_host.SetPublisherForTesting(&mock_app_publisher);
  web_apps_publisher_host.Init();
  mock_app_publisher.Wait();

  EXPECT_EQ(mock_app_publisher.get_deltas().back()->publisher_id,
            app_url.spec());
  EXPECT_TRUE(mock_app_publisher.get_deltas().back()->policy_id->empty());

  // Set policy to pin the web app.
  web_app::ExternallyInstalledWebAppPrefs web_app_prefs(
      browser()->profile()->GetPrefs());
  web_app_prefs.Insert(install_url, app_id,
                       web_app::ExternalInstallSource::kExternalPolicy);

  provider().registrar().NotifyWebAppInstalledWithOsHooks(app_id);

  mock_app_publisher.Wait();
  EXPECT_EQ(mock_app_publisher.get_deltas().back()->policy_id,
            install_url.spec());
}

IN_PROC_BROWSER_TEST_F(WebAppsPublisherHostBrowserTest, ContentSettings) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL app_url = embedded_test_server()->GetURL("/web_apps/basic.html");
  AppId app_id = InstallWebAppFromManifest(browser(), app_url);

  // Install an additional app from a different host.
  {
    auto web_app_info = std::make_unique<WebApplicationInfo>();
    web_app_info->start_url = GURL("https://example.com:8080/");
    web_app_info->scope = web_app_info->start_url;
    web_app_info->title = u"Unrelated Web App";
    InstallWebApp(std::move(web_app_info));
  }

  MockAppPublisher mock_app_publisher;
  WebAppsPublisherHost web_apps_publisher_host(profile());
  web_apps_publisher_host.SetPublisherForTesting(&mock_app_publisher);
  web_apps_publisher_host.Init();
  mock_app_publisher.Wait();
  EXPECT_EQ(mock_app_publisher.get_deltas().size(), 2U);

  HostContentSettingsMap* content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile());
  content_settings_map->SetContentSettingDefaultScope(
      app_url, app_url, ContentSettingsType::MEDIASTREAM_CAMERA,
      CONTENT_SETTING_ALLOW);
  mock_app_publisher.Wait();
  EXPECT_EQ(mock_app_publisher.get_deltas().size(), 3U);
  EXPECT_EQ(mock_app_publisher.get_deltas().back()->app_type,
            apps::mojom::AppType::kWeb);
  EXPECT_EQ(mock_app_publisher.get_deltas().back()->app_id, app_id);

  const std::vector<apps::mojom::PermissionPtr>& permissions =
      mock_app_publisher.get_deltas().back()->permissions;
  auto camera_permission =
      std::find_if(permissions.begin(), permissions.end(),
                   [](const apps::mojom::PermissionPtr& permission) {
                     return permission->permission_type ==
                            apps::mojom::PermissionType::kCamera;
                   });
  ASSERT_TRUE(camera_permission != permissions.end());
  EXPECT_TRUE((*camera_permission)->value->is_tristate_value());
  EXPECT_EQ((*camera_permission)->value->get_tristate_value(),
            apps::mojom::TriState::kAllow);
}

IN_PROC_BROWSER_TEST_F(WebAppsPublisherHostBrowserTest, MediaRequest) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL app_url = embedded_test_server()->GetURL("/web_apps/basic.html");
  AppId app_id = InstallWebAppFromManifest(browser(), app_url);
  Browser* browser = LaunchWebAppBrowserAndWait(app_id);
  content::RenderFrameHost* render_frame_host =
      browser->tab_strip_model()->GetActiveWebContents()->GetMainFrame();
  const int render_process_id = render_frame_host->GetProcess()->GetID();
  const int render_frame_id = render_frame_host->GetRoutingID();

  MockAppPublisher mock_app_publisher;
  WebAppsPublisherHost& web_apps_publisher_host =
      *apps::AppServiceProxyFactory::GetForProfile(profile())
           ->WebAppsPublisherHostForTesting();
  web_apps_publisher_host.SetPublisherForTesting(&mock_app_publisher);

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindLambdaForTesting([render_process_id, render_frame_id,
                                             app_url]() {
        MediaCaptureDevicesDispatcher::GetInstance()
            ->OnMediaRequestStateChanged(
                render_process_id, render_frame_id,
                /*page_request_id=*/0, app_url,
                blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE,
                content::MEDIA_REQUEST_STATE_DONE);
      }));
  mock_app_publisher.Wait();
  EXPECT_EQ(mock_app_publisher.get_capability_access_deltas().size(), 1U);
  EXPECT_EQ(mock_app_publisher.get_capability_access_deltas().back()->app_id,
            app_id);
  EXPECT_EQ(mock_app_publisher.get_capability_access_deltas().back()->camera,
            apps::mojom::OptionalBool::kUnknown);
  EXPECT_EQ(
      mock_app_publisher.get_capability_access_deltas().back()->microphone,
      apps::mojom::OptionalBool::kTrue);

  browser->tab_strip_model()->CloseAllTabs();
  mock_app_publisher.Wait();
  EXPECT_EQ(mock_app_publisher.get_capability_access_deltas().size(), 2U);
  EXPECT_EQ(mock_app_publisher.get_capability_access_deltas().back()->app_id,
            app_id);
  EXPECT_EQ(mock_app_publisher.get_capability_access_deltas().back()->camera,
            apps::mojom::OptionalBool::kUnknown);
  EXPECT_EQ(
      mock_app_publisher.get_capability_access_deltas().back()->microphone,
      apps::mojom::OptionalBool::kFalse);
}

IN_PROC_BROWSER_TEST_F(WebAppsPublisherHostBrowserTest, Launch) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL app_url = embedded_test_server()->GetURL("/web_apps/basic.html");
  AppId app_id = InstallWebAppFromManifest(browser(), app_url);

  MockAppPublisher mock_app_publisher;
  WebAppsPublisherHost web_apps_publisher_host(profile());
  web_apps_publisher_host.SetPublisherForTesting(&mock_app_publisher);
  web_apps_publisher_host.Init();
  mock_app_publisher.Wait();

  content::TestNavigationObserver navigation_observer(app_url);
  navigation_observer.StartWatchingNewWebContents();
  auto launch_params = crosapi::mojom::LaunchParams::New();
  launch_params->app_id = app_id;
  launch_params->launch_source = apps::mojom::LaunchSource::kFromTest;
  web_apps_publisher_host.Launch(std::move(launch_params), base::DoNothing());
  navigation_observer.Wait();
}

IN_PROC_BROWSER_TEST_F(WebAppsPublisherHostBrowserTest, PauseUnpause) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL app_url = embedded_test_server()->GetURL("/web_apps/basic.html");
  const AppId app_id = InstallWebAppFromManifest(browser(), app_url);
  LaunchWebAppBrowserAndWait(app_id);

  MockAppPublisher mock_app_publisher;
  WebAppsPublisherHost web_apps_publisher_host(profile());
  web_apps_publisher_host.SetPublisherForTesting(&mock_app_publisher);
  web_apps_publisher_host.Init();
  mock_app_publisher.Wait();
  EXPECT_EQ(mock_app_publisher.get_deltas().size(), 1U);

  web_apps_publisher_host.PauseApp(app_id);
  mock_app_publisher.Wait();
  EXPECT_EQ(mock_app_publisher.get_deltas().size(), 3U);

  EXPECT_EQ(mock_app_publisher.get_deltas()[1]->app_type,
            apps::mojom::AppType::kWeb);
  EXPECT_EQ(mock_app_publisher.get_deltas()[1]->app_id, app_id);
  EXPECT_EQ(mock_app_publisher.get_deltas()[1]->icon_key->icon_effects,
            IconEffects::kRoundCorners | IconEffects::kCrOsStandardIcon |
                IconEffects::kPaused);

  EXPECT_EQ(mock_app_publisher.get_deltas().back()->app_type,
            apps::mojom::AppType::kWeb);
  EXPECT_EQ(mock_app_publisher.get_deltas().back()->app_id, app_id);
  EXPECT_EQ(mock_app_publisher.get_deltas().back()->paused,
            apps::mojom::OptionalBool::kTrue);

  web_apps_publisher_host.UnpauseApp(app_id);
  mock_app_publisher.Wait();
  EXPECT_EQ(mock_app_publisher.get_deltas().size(), 5U);

  EXPECT_EQ(mock_app_publisher.get_deltas()[3]->app_type,
            apps::mojom::AppType::kWeb);
  EXPECT_EQ(mock_app_publisher.get_deltas()[3]->app_id, app_id);
  EXPECT_EQ(mock_app_publisher.get_deltas()[3]->icon_key->icon_effects,
            IconEffects::kRoundCorners | IconEffects::kCrOsStandardIcon);

  EXPECT_EQ(mock_app_publisher.get_deltas().back()->app_type,
            apps::mojom::AppType::kWeb);
  EXPECT_EQ(mock_app_publisher.get_deltas().back()->app_id, app_id);
  EXPECT_EQ(mock_app_publisher.get_deltas().back()->paused,
            apps::mojom::OptionalBool::kFalse);
}

IN_PROC_BROWSER_TEST_F(WebAppsPublisherHostBrowserTest, GetMenuModel) {
  auto CheckMenuItem = [](const crosapi::mojom::MenuItem& menu_item,
                          const std::string& label,
                          absl::optional<SkColor> color) {
    EXPECT_EQ(menu_item.label, label);
    if (color.has_value()) {
      EXPECT_FALSE(menu_item.image.isNull());
      EXPECT_EQ(menu_item.image.bitmap()->getColor(15, 15), color);
    } else {
      EXPECT_TRUE(menu_item.image.isNull());
    }
  };

  const GURL app_url =
      https_server()->GetURL("/web_app_shortcuts/shortcuts.html");
  const web_app::AppId app_id =
      web_app::InstallWebAppFromPage(browser(), app_url);
  crosapi::mojom::AppController& web_apps_publisher_host =
      *apps::AppServiceProxyFactory::GetForProfile(profile())
           ->WebAppsPublisherHostForTesting();

  crosapi::mojom::MenuItemsPtr menu_items;
  {
    base::RunLoop run_loop;
    web_apps_publisher_host.GetMenuModel(
        app_id,
        base::BindLambdaForTesting(
            [&run_loop, &menu_items](crosapi::mojom::MenuItemsPtr items) {
              menu_items = std::move(items);
              run_loop.Quit();
            }));
    run_loop.Run();
  }

  ASSERT_TRUE(menu_items);
  ASSERT_EQ(6U, menu_items->items.size());
  CheckMenuItem(*menu_items->items[0], "One", SK_ColorGREEN);
  CheckMenuItem(*menu_items->items[1], "Two", SK_ColorBLUE);
  CheckMenuItem(*menu_items->items[2], "Three", SK_ColorYELLOW);
  CheckMenuItem(*menu_items->items[3], "Four", SK_ColorCYAN);
  CheckMenuItem(*menu_items->items[4], "Five", SK_ColorMAGENTA);
  CheckMenuItem(*menu_items->items[5], "Six", absl::nullopt);
}

IN_PROC_BROWSER_TEST_F(WebAppsPublisherHostBrowserTest,
                       ExecuteContextMenuCommand) {
  const GURL app_url =
      https_server()->GetURL("/web_app_shortcuts/shortcuts.html");
  const web_app::AppId app_id =
      web_app::InstallWebAppFromPage(browser(), app_url);
  WebAppsPublisherHost& web_apps_publisher_host =
      *apps::AppServiceProxyFactory::GetForProfile(profile())
           ->WebAppsPublisherHostForTesting();

  crosapi::mojom::MenuItemsPtr menu_items;
  {
    base::RunLoop run_loop;
    web_apps_publisher_host.GetMenuModel(
        app_id,
        base::BindLambdaForTesting(
            [&run_loop, &menu_items](crosapi::mojom::MenuItemsPtr items) {
              menu_items = std::move(items);
              run_loop.Quit();
            }));
    run_loop.Run();
  }
  ASSERT_TRUE(menu_items);
  ASSERT_EQ(6U, menu_items->items.size());

  auto id = *menu_items->items[5]->id;

  web_apps_publisher_host.ExecuteContextMenuCommand(app_id, id,
                                                    base::DoNothing());

  EXPECT_EQ(BrowserList::GetInstance()
                ->GetLastActive()
                ->tab_strip_model()
                ->GetActiveWebContents()
                ->GetVisibleURL(),
            https_server()->GetURL("/web_app_shortcuts/shortcuts.html#six"));
}

IN_PROC_BROWSER_TEST_F(WebAppsPublisherHostBrowserTest, OpenNativeSettings) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL app_url = embedded_test_server()->GetURL("/web_apps/basic.html");
  AppId app_id = InstallWebAppFromManifest(browser(), app_url);

  MockAppPublisher mock_app_publisher;
  WebAppsPublisherHost web_apps_publisher_host(profile());
  web_apps_publisher_host.SetPublisherForTesting(&mock_app_publisher);
  web_apps_publisher_host.Init();
  mock_app_publisher.Wait();

  web_apps_publisher_host.OpenNativeSettings(app_id);
  content::WebContents* const web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(base::StartsWith(web_contents->GetVisibleURL().spec(),
                               chrome::kChromeUIContentSettingsURL,
                               base::CompareCase::SENSITIVE));
}

IN_PROC_BROWSER_TEST_F(WebAppsPublisherHostBrowserTest, WindowMode) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL app_url = embedded_test_server()->GetURL("/web_apps/basic.html");
  AppId app_id = InstallWebAppFromManifest(browser(), app_url);

  MockAppPublisher mock_app_publisher;
  WebAppsPublisherHost web_apps_publisher_host(profile());
  web_apps_publisher_host.SetPublisherForTesting(&mock_app_publisher);
  web_apps_publisher_host.Init();
  mock_app_publisher.Wait();

  EXPECT_EQ(mock_app_publisher.get_deltas().size(), 1U);
  EXPECT_EQ(mock_app_publisher.get_deltas().back()->window_mode,
            apps::mojom::WindowMode::kWindow);

  web_apps_publisher_host.SetWindowMode(app_id,
                                        apps::mojom::WindowMode::kBrowser);
  mock_app_publisher.Wait();

  EXPECT_GE(mock_app_publisher.get_deltas().size(), 2U);
  EXPECT_EQ(mock_app_publisher.get_deltas().back()->window_mode,
            apps::mojom::WindowMode::kBrowser);
}

IN_PROC_BROWSER_TEST_F(WebAppsPublisherHostBrowserTest, Notification) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL app_url = embedded_test_server()->GetURL("/web_apps/basic.html");
  AppId app_id = InstallWebAppFromManifest(browser(), app_url);

  MockAppPublisher mock_app_publisher;
  WebAppsPublisherHost web_apps_publisher_host(profile());
  web_apps_publisher_host.SetPublisherForTesting(&mock_app_publisher);
  web_apps_publisher_host.Init();
  mock_app_publisher.Wait();
  EXPECT_EQ(mock_app_publisher.get_deltas().size(), 1U);
  EXPECT_EQ(mock_app_publisher.get_deltas().back()->has_badge,
            apps::mojom::OptionalBool::kFalse);

  const GURL origin = app_url.DeprecatedGetOriginAsURL();
  const std::string notification_id = "notification-id";
  auto notification = std::make_unique<message_center::Notification>(
      message_center::NOTIFICATION_TYPE_SIMPLE, notification_id,
      std::u16string(), std::u16string(), gfx::Image(),
      base::UTF8ToUTF16(origin.host()), origin,
      message_center::NotifierId(origin),
      message_center::RichNotificationData(), nullptr);
  auto metadata = std::make_unique<PersistentNotificationMetadata>();
  metadata->service_worker_scope = app_url.GetWithoutFilename();

  NotificationDisplayService::GetForProfile(profile())->Display(
      NotificationHandler::Type::WEB_PERSISTENT, *notification,
      std::move(metadata));
  mock_app_publisher.Wait();
  EXPECT_EQ(mock_app_publisher.get_deltas().size(), 2U);
  EXPECT_EQ(mock_app_publisher.get_deltas().back()->has_badge,
            apps::mojom::OptionalBool::kTrue);
}

IN_PROC_BROWSER_TEST_F(WebAppsPublisherHostBrowserTest, DisabledState) {
  ASSERT_TRUE(embedded_test_server()->Start());
  WebAppSyncBridge& web_app_sync_bridge = provider().sync_bridge();
  const AppId app_id = InstallWebAppFromManifest(
      browser(), embedded_test_server()->GetURL("/web_apps/basic.html"));

  AppId app2_id;
  {
    const std::u16string description = u"Uninstalled Web App";
    auto web_app_info = std::make_unique<WebApplicationInfo>();
    web_app_info->start_url =
        embedded_test_server()->GetURL("app.site.com", "/simple.html");
    web_app_info->scope = web_app_info->start_url.GetWithoutFilename();
    web_app_info->title = description;
    web_app_info->description = description;
    app2_id = InstallWebApp(std::move(web_app_info));
    web_app_sync_bridge.SetAppIsLocallyInstalled(
        app2_id,
        /*is_locally_installed=*/false);
  }

  MockAppPublisher mock_app_publisher;
  WebAppsPublisherHost web_apps_publisher_host(profile());
  web_apps_publisher_host.SetPublisherForTesting(&mock_app_publisher);
  web_apps_publisher_host.Init();
  mock_app_publisher.Wait();
  EXPECT_EQ(mock_app_publisher.get_deltas().size(), 2U);
  if (mock_app_publisher.get_deltas()[0]->app_id == app_id) {
    EXPECT_EQ(mock_app_publisher.get_deltas()[0]->icon_key->icon_effects,
              IconEffects::kRoundCorners | IconEffects::kCrOsStandardIcon);
    EXPECT_EQ(mock_app_publisher.get_deltas()[1]->app_id, app2_id);
    EXPECT_EQ(mock_app_publisher.get_deltas()[1]->icon_key->icon_effects,
              IconEffects::kRoundCorners | IconEffects::kCrOsStandardMask |
                  IconEffects::kBlocked);
  } else {
    EXPECT_EQ(mock_app_publisher.get_deltas()[0]->app_id, app2_id);
    EXPECT_EQ(mock_app_publisher.get_deltas()[0]->icon_key->icon_effects,
              IconEffects::kRoundCorners | IconEffects::kCrOsStandardMask |
                  IconEffects::kBlocked);
    EXPECT_EQ(mock_app_publisher.get_deltas()[1]->app_id, app_id);
    EXPECT_EQ(mock_app_publisher.get_deltas()[1]->icon_key->icon_effects,
              IconEffects::kRoundCorners | IconEffects::kCrOsStandardIcon);
  }

  web_app_sync_bridge.SetAppIsDisabled(app_id, true);
  mock_app_publisher.Wait();
  EXPECT_EQ(mock_app_publisher.get_deltas().size(), 3U);
  EXPECT_EQ(mock_app_publisher.get_deltas().back()->app_id, app_id);
  EXPECT_EQ(mock_app_publisher.get_deltas().back()->icon_key->icon_effects,
            IconEffects::kRoundCorners | IconEffects::kCrOsStandardIcon |
                IconEffects::kBlocked);

  web_app_sync_bridge.SetAppIsDisabled(app2_id, true);
  mock_app_publisher.Wait();
  EXPECT_EQ(mock_app_publisher.get_deltas().size(), 4U);
  EXPECT_EQ(mock_app_publisher.get_deltas().back()->app_id, app2_id);
  EXPECT_EQ(mock_app_publisher.get_deltas().back()->icon_key->icon_effects,
            IconEffects::kRoundCorners | IconEffects::kCrOsStandardMask |
                IconEffects::kBlocked);

  web_app_sync_bridge.SetAppIsDisabled(app_id, false);
  mock_app_publisher.Wait();
  EXPECT_EQ(mock_app_publisher.get_deltas().size(), 5U);
  EXPECT_EQ(mock_app_publisher.get_deltas().back()->app_id, app_id);
  EXPECT_EQ(mock_app_publisher.get_deltas().back()->icon_key->icon_effects,
            IconEffects::kRoundCorners | IconEffects::kCrOsStandardIcon);

  web_app_sync_bridge.SetAppIsDisabled(app2_id, false);
  mock_app_publisher.Wait();
  EXPECT_EQ(mock_app_publisher.get_deltas().size(), 6U);
  EXPECT_EQ(mock_app_publisher.get_deltas().back()->app_id, app2_id);
  EXPECT_EQ(mock_app_publisher.get_deltas().back()->icon_key->icon_effects,
            IconEffects::kRoundCorners | IconEffects::kCrOsStandardMask |
                IconEffects::kBlocked);

  provider().registrar().NotifyWebAppManifestUpdated(app_id,
                                                     base::StringPiece());
  mock_app_publisher.Wait();
  EXPECT_EQ(mock_app_publisher.get_deltas().size(), 7U);
  EXPECT_EQ(mock_app_publisher.get_deltas().back()->app_id, app_id);
  EXPECT_EQ(mock_app_publisher.get_deltas().back()->icon_key->icon_effects,
            IconEffects::kRoundCorners | IconEffects::kCrOsStandardIcon);
}

}  // namespace web_app
