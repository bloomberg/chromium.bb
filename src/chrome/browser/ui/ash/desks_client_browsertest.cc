// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/desks_client.h"

#include <cstdint>
#include <cstdlib>
#include <memory>
#include <string>

#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/desk_template.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/wm/desks/desks_test_util.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/ui/user_adding_screen.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_keep_alive_types.h"
#include "chrome/browser/profiles/scoped_profile_keep_alive.h"
#include "chrome/browser/ui/ash/desk_template_app_launch_handler.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/web_applications/system_web_app_ui_utils.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_types.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/ui/base/window_state_type.h"
#include "components/app_restore/app_launch_info.h"
#include "components/app_restore/features.h"
#include "components/app_restore/full_restore_utils.h"
#include "components/app_restore/restore_data.h"
#include "components/app_restore/window_properties.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/focus_client.h"
#include "ui/display/screen.h"
#include "url/gurl.h"

using ::testing::_;
using ::testing::ElementsAre;

namespace {

constexpr int32_t kSettingsWindowId = 100;

constexpr char kExampleUrl1[] = "https://examples1.com";
constexpr char kExampleUrl2[] = "https://examples2.com";
constexpr char kExampleUrl3[] = "https://examples3.com";
constexpr char kYoutubeUrl[] = "https://www.youtube.com/";

Browser* FindBrowser(int32_t window_id) {
  for (auto* browser : *BrowserList::GetInstance()) {
    aura::Window* window = browser->window()->GetNativeWindow();
    if (window->GetProperty(app_restore::kRestoreWindowIdKey) == window_id)
      return browser;
  }
  return nullptr;
}

aura::Window* FindBrowserWindow(int32_t window_id) {
  Browser* browser = FindBrowser(window_id);
  return browser ? browser->window()->GetNativeWindow() : nullptr;
}

std::vector<GURL> GetURLsForBrowserWindow(Browser* browser) {
  TabStripModel* tab_strip_model = browser->tab_strip_model();
  std::vector<GURL> urls;
  for (int i = 0; i < tab_strip_model->count(); ++i)
    urls.push_back(tab_strip_model->GetWebContentsAt(i)->GetLastCommittedURL());
  return urls;
}

std::unique_ptr<ash::DeskTemplate> CaptureActiveDeskAndSaveTemplate() {
  base::RunLoop run_loop;
  std::unique_ptr<ash::DeskTemplate> desk_template;
  DesksClient::Get()->CaptureActiveDeskAndSaveTemplate(
      base::BindLambdaForTesting(
          [&](std::unique_ptr<ash::DeskTemplate> captured_desk_template,
              std::string error_string) {
            run_loop.Quit();
            ASSERT_TRUE(captured_desk_template);
            desk_template = std::move(captured_desk_template);
          }));
  run_loop.Run();
  return desk_template;
}

void DeleteDeskTemplate(const base::GUID uuid) {
  base::RunLoop run_loop;
  DesksClient::Get()->DeleteDeskTemplate(
      uuid.AsLowercaseString(),
      base::BindLambdaForTesting(
          [&](std::string error_string) { run_loop.Quit(); }));
  run_loop.Run();
}

web_app::AppId CreateSettingsSystemWebApp(Profile* profile) {
  web_app::WebAppProvider::GetForTest(profile)
      ->system_web_app_manager()
      .InstallSystemAppsForTesting();
  web_app::AppId settings_app_id = *web_app::GetAppIdForSystemWebApp(
      profile, web_app::SystemAppType::SETTINGS);
  apps::AppLaunchParams params(
      settings_app_id, apps::mojom::LaunchContainer::kLaunchContainerWindow,
      WindowOpenDisposition::NEW_WINDOW,
      apps::mojom::AppLaunchSource::kSourceTest);
  params.restore_id = kSettingsWindowId;
  apps::AppServiceProxyFactory::GetForProfile(profile)
      ->BrowserAppLauncher()
      ->LaunchAppWithParams(std::move(params));
  web_app::FlushSystemWebAppLaunchesForTesting(profile);
  return settings_app_id;
}

class MockDeskTemplateAppLaunchHandler : public DeskTemplateAppLaunchHandler {
 public:
  explicit MockDeskTemplateAppLaunchHandler(Profile* profile)
      : DeskTemplateAppLaunchHandler(profile) {}
  MockDeskTemplateAppLaunchHandler(const MockDeskTemplateAppLaunchHandler&) =
      delete;
  MockDeskTemplateAppLaunchHandler& operator=(
      const MockDeskTemplateAppLaunchHandler&) = delete;
  ~MockDeskTemplateAppLaunchHandler() override = default;

  MOCK_METHOD(void,
              LaunchSystemWebAppOrChromeApp,
              (apps::mojom::AppType,
               const std::string&,
               const app_restore::RestoreData::LaunchList&),
              (override));
};

}  // namespace

// Scoped class that temporarily sets a new app launch handler for testing
// purposes.
class ScopedDeskClientAppLaunchHandlerSetter {
 public:
  explicit ScopedDeskClientAppLaunchHandlerSetter(
      std::unique_ptr<DeskTemplateAppLaunchHandler> launch_handler) {
    DCHECK_EQ(0, instance_count_);
    ++instance_count_;

    DesksClient* desks_client = DesksClient::Get();
    DCHECK(desks_client);
    old_app_launch_handler_ = std::move(desks_client->app_launch_handler_);
    desks_client->app_launch_handler_ = std::move(launch_handler);
  }
  ScopedDeskClientAppLaunchHandlerSetter(
      const ScopedDeskClientAppLaunchHandlerSetter&) = delete;
  ScopedDeskClientAppLaunchHandlerSetter& operator=(
      const ScopedDeskClientAppLaunchHandlerSetter&) = delete;
  ~ScopedDeskClientAppLaunchHandlerSetter() {
    DCHECK_EQ(1, instance_count_);
    --instance_count_;

    DesksClient* desks_client = DesksClient::Get();
    DCHECK(desks_client);
    desks_client->app_launch_handler_ = std::move(old_app_launch_handler_);
  }

 private:
  // Variable to ensure we never have more than one instance of this object.
  static int instance_count_;

  // The old app launch handler prior to the object being created. May be
  // nullptr.
  std::unique_ptr<DeskTemplateAppLaunchHandler> old_app_launch_handler_;
};

int ScopedDeskClientAppLaunchHandlerSetter::instance_count_ = 0;

class DesksClientTest : public extensions::PlatformAppBrowserTest {
 public:
  DesksClientTest() {
    // This feature depends on full restore feature, so need to enable it.
    scoped_feature_list_.InitAndEnableFeature(
        full_restore::features::kFullRestore);
  }
  ~DesksClientTest() override = default;

  void SetUpOnMainThread() override {
    ::full_restore::SetActiveProfilePath(profile()->GetPath());
    extensions::PlatformAppBrowserTest::SetUpOnMainThread();
  }

  void SetTemplate(std::unique_ptr<ash::DeskTemplate> launch_template) {
    DesksClient::Get()->launch_template_for_test_ = std::move(launch_template);
  }

  void LaunchTemplate(const base::GUID& uuid) {
    ash::DeskSwitchAnimationWaiter waiter;
    DesksClient::Get()->LaunchDeskTemplate(uuid.AsLowercaseString(),
                                           base::DoNothing());
    waiter.Wait();
  }

  void SetAndLaunchTemplate(std::unique_ptr<ash::DeskTemplate> desk_template) {
    ash::DeskTemplate* desk_template_ptr = desk_template.get();
    SetTemplate(std::move(desk_template));
    LaunchTemplate(desk_template_ptr->uuid());
  }

  Browser* CreateBrowser(const std::vector<GURL>& urls,
                         absl::optional<int> active_url_index = absl::nullopt) {
    Browser::CreateParams params(Browser::TYPE_NORMAL, profile(),
                                 /*user_gesture=*/false);
    Browser* browser = Browser::Create(params);
    for (int i = 0; i < urls.size(); i++) {
      chrome::AddTabAt(
          browser, urls[i], /*index=*/-1,
          /*foreground=*/!active_url_index || active_url_index.value() == i);
    }
    browser->window()->Show();
    return browser;
  }

  Browser* InstallAndLaunchPWA(const GURL& start_url, bool launch_in_browser) {
    auto web_app_info = std::make_unique<WebApplicationInfo>();
    web_app_info->start_url = start_url;
    web_app_info->scope = start_url.GetWithoutFilename();
    if (!launch_in_browser)
      web_app_info->user_display_mode = blink::mojom::DisplayMode::kStandalone;
    web_app_info->title = u"A Web App";
    const web_app::AppId app_id =
        web_app::test::InstallWebApp(profile(), std::move(web_app_info));

    // Wait for app service to see the newly installed app.
    auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile());
    proxy->FlushMojoCallsForTesting();

    return launch_in_browser
               ? web_app::LaunchBrowserForWebAppInTab(profile(), app_id)
               : web_app::LaunchWebAppBrowserAndWait(profile(), app_id);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that a browser's urls can be captured correctly in the desk template.
IN_PROC_BROWSER_TEST_F(DesksClientTest, CaptureBrowserUrlsTest) {
  // Create a new browser and add a few tabs to it.
  Browser* browser = CreateBrowser({GURL(kExampleUrl1), GURL(kExampleUrl2)});
  aura::Window* window = browser->window()->GetNativeWindow();

  const int32_t browser_window_id =
      window->GetProperty(app_restore::kWindowIdKey);
  // Get current tabs from browser.
  std::vector<GURL> urls = GetURLsForBrowserWindow(browser);

  std::unique_ptr<ash::DeskTemplate> desk_template =
      CaptureActiveDeskAndSaveTemplate();
  const app_restore::RestoreData* restore_data =
      desk_template->desk_restore_data();
  const auto& app_id_to_launch_list = restore_data->app_id_to_launch_list();
  EXPECT_EQ(app_id_to_launch_list.size(), 1u);

  // Find |browser| window's app restore data.
  auto iter = app_id_to_launch_list.find(extension_misc::kChromeAppId);
  ASSERT_TRUE(iter != app_id_to_launch_list.end());
  auto app_restore_data_iter = iter->second.find(browser_window_id);
  ASSERT_TRUE(app_restore_data_iter != iter->second.end());
  const auto& data = app_restore_data_iter->second;
  // Check the urls are captured correctly in the |desk_template|.
  EXPECT_EQ(data->urls.value(), urls);
}

// Tests that incognito browser windows will NOT be captured in the desk
// template.
IN_PROC_BROWSER_TEST_F(DesksClientTest, CaptureIncognitoBrowserTest) {
  Browser* incognito_browser = CreateIncognitoBrowser();
  chrome::AddTabAt(incognito_browser, GURL(kExampleUrl1), /*index=*/-1,
                   /*foreground=*/true);
  chrome::AddTabAt(incognito_browser, GURL(kExampleUrl2), /*index=*/-1,
                   /*foreground=*/true);
  incognito_browser->window()->Show();
  aura::Window* window = incognito_browser->window()->GetNativeWindow();

  const int32_t incognito_browser_window_id =
      window->GetProperty(app_restore::kWindowIdKey);

  std::unique_ptr<ash::DeskTemplate> desk_template =
      CaptureActiveDeskAndSaveTemplate();
  ASSERT_TRUE(desk_template);
  const app_restore::RestoreData* restore_data =
      desk_template->desk_restore_data();
  const auto& app_id_to_launch_list = restore_data->app_id_to_launch_list();
  EXPECT_EQ(app_id_to_launch_list.size(), 1u);

  // Find |browser| window's app restore data.
  auto iter = app_id_to_launch_list.find(extension_misc::kChromeAppId);
  ASSERT_TRUE(iter != app_id_to_launch_list.end());
  auto app_restore_data_iter = iter->second.find(incognito_browser_window_id);
  // Created incognito window is NOT in restore list
  ASSERT_TRUE(app_restore_data_iter == iter->second.end());
}

// Tests that browsers and chrome apps can be captured correctly in the desk
// template.
IN_PROC_BROWSER_TEST_F(DesksClientTest, CaptureActiveDeskAsTemplateTest) {
  // Test that Singleton was properly initialized.
  ASSERT_TRUE(DesksClient::Get());

  // Change |browser|'s bounds.
  const gfx::Rect browser_bounds = gfx::Rect(0, 0, 800, 200);
  aura::Window* window = browser()->window()->GetNativeWindow();
  window->SetBounds(browser_bounds);
  // Make window visible on all desks.
  window->SetProperty(aura::client::kWindowWorkspaceKey,
                      aura::client::kWindowWorkspaceVisibleOnAllWorkspaces);
  const int32_t browser_window_id =
      window->GetProperty(app_restore::kWindowIdKey);

  // Create the settings app, which is a system web app.
  web_app::AppId settings_app_id =
      CreateSettingsSystemWebApp(browser()->profile());

  // Change the Settings app's bounds too.
  const gfx::Rect settings_app_bounds = gfx::Rect(100, 100, 800, 300);
  aura::Window* settings_window = FindBrowserWindow(kSettingsWindowId);
  const int32_t settings_window_id =
      settings_window->GetProperty(app_restore::kWindowIdKey);
  ASSERT_TRUE(settings_window);
  settings_window->SetBounds(settings_app_bounds);

  std::unique_ptr<ash::DeskTemplate> desk_template =
      CaptureActiveDeskAndSaveTemplate();
  // Test the default template's name is the current desk's name.
  auto* desks_controller = ash::DesksController::Get();
  EXPECT_EQ(
      desk_template->template_name(),
      desks_controller->GetDeskName(desks_controller->GetActiveDeskIndex()));

  const app_restore::RestoreData* restore_data =
      desk_template->desk_restore_data();
  const auto& app_id_to_launch_list = restore_data->app_id_to_launch_list();
  EXPECT_EQ(app_id_to_launch_list.size(), 2u);

  // Find |browser| window's app restore data.
  auto iter = app_id_to_launch_list.find(extension_misc::kChromeAppId);
  ASSERT_TRUE(iter != app_id_to_launch_list.end());
  auto app_restore_data_iter = iter->second.find(browser_window_id);
  ASSERT_TRUE(app_restore_data_iter != iter->second.end());
  const auto& data = app_restore_data_iter->second;
  // Verify window info are correctly captured.
  EXPECT_EQ(browser_bounds, data->current_bounds.value());
  // `visible_on_all_workspaces` should have been reset even though
  // the captured window is visible on all workspaces.
  EXPECT_FALSE(data->desk_id.has_value());
  auto* screen = display::Screen::GetScreen();
  EXPECT_EQ(screen->GetDisplayNearestWindow(window).id(),
            data->display_id.value());
  EXPECT_EQ(window->GetProperty(aura::client::kShowStateKey),
            chromeos::ToWindowShowState(data->window_state_type.value()));
  // We don't capture the window's desk_id as a template will always
  // create in a new desk.
  EXPECT_FALSE(data->desk_id.has_value());

  // Find Setting app's app restore data.
  auto iter2 = app_id_to_launch_list.find(settings_app_id);
  ASSERT_TRUE(iter2 != app_id_to_launch_list.end());
  auto app_restore_data_iter2 = iter2->second.find(settings_window_id);
  ASSERT_TRUE(app_restore_data_iter2 != iter2->second.end());
  const auto& data2 = app_restore_data_iter2->second;
  EXPECT_EQ(
      static_cast<int>(apps::mojom::LaunchContainer::kLaunchContainerWindow),
      data2->container.value());
  EXPECT_EQ(static_cast<int>(WindowOpenDisposition::NEW_WINDOW),
            data2->disposition.value());
  // Verify window info are correctly captured.
  EXPECT_EQ(settings_app_bounds, data2->current_bounds.value());
  EXPECT_FALSE(data2->desk_id.has_value());
  EXPECT_EQ(screen->GetDisplayNearestWindow(window).id(),
            data->display_id.value());
  EXPECT_EQ(window->GetProperty(aura::client::kShowStateKey),
            chromeos::ToWindowShowState(data->window_state_type.value()));
  EXPECT_EQ(window->GetProperty(aura::client::kShowStateKey),
            chromeos::ToWindowShowState(data->window_state_type.value()));
  EXPECT_FALSE(data2->desk_id.has_value());
}

// Tests that launching a desk template creates a desk with the given name.
IN_PROC_BROWSER_TEST_F(DesksClientTest, LaunchEmptyDeskTemplate) {
  const base::GUID kDeskUuid = base::GUID::GenerateRandomV4();
  const std::u16string kDeskName(u"Test Desk Name");

  auto* desks_controller = ash::DesksController::Get();

  ASSERT_EQ(0, desks_controller->GetActiveDeskIndex());

  auto desk_template = std::make_unique<ash::DeskTemplate>(kDeskUuid);
  desk_template->set_template_name(kDeskName);
  SetAndLaunchTemplate(std::move(desk_template));

  EXPECT_EQ(1, desks_controller->GetActiveDeskIndex());
  EXPECT_EQ(kDeskName, desks_controller->GetDeskName(1));

  // Verify that user prefs are updated with the correct desk names. This
  // ensures that template created desk names are preserved on restart.
  PrefService* primary_user_prefs =
      ash::Shell::Get()->session_controller()->GetPrimaryUserPrefService();
  ASSERT_TRUE(primary_user_prefs);
  const base::ListValue* desks_names =
      primary_user_prefs->GetList(ash::prefs::kDesksNamesList);
  const auto& desks_names_list = desks_names->GetList();
  EXPECT_EQ(2, desks_names->GetList().size());
  EXPECT_EQ(base::UTF16ToUTF8(kDeskName), desks_names_list[1].GetString());
}

// Tests that launching the same desk template multiple times creates desks with
// different/incremented names.
IN_PROC_BROWSER_TEST_F(DesksClientTest, LaunchMultipleEmptyDeskTemplates) {
  const base::GUID kDeskUuid = base::GUID::GenerateRandomV4();
  const std::u16string kDeskName(u"Test Desk Name");

  auto* desks_controller = ash::DesksController::Get();

  ASSERT_EQ(0, desks_controller->GetActiveDeskIndex());

  auto desk_template = std::make_unique<ash::DeskTemplate>(kDeskUuid);
  desk_template->set_template_name(kDeskName);
  SetTemplate(std::move(desk_template));

  auto check_launch_template_desk_name =
      [kDeskUuid, desks_controller, this](const std::u16string& desk_name) {
        LaunchTemplate(kDeskUuid);

        EXPECT_EQ(desk_name, desks_controller->GetDeskName(
                                 desks_controller->GetActiveDeskIndex()));
      };

  // Launching a desk from the template creates a desk with the same name as the
  // template.
  check_launch_template_desk_name(kDeskName);

  // Launch more desks from the template and verify that the newly created desks
  // have unique names.
  check_launch_template_desk_name(std::u16string(kDeskName).append(u" (1)"));
  check_launch_template_desk_name(std::u16string(kDeskName).append(u" (2)"));

  // Remove "Test Desk Name (1)", which means the next created desk from
  // template will have that name. Then it will skip (2) since it already
  // exists, and create the next desk with (3).
  RemoveDesk(desks_controller->desks()[2].get());
  check_launch_template_desk_name(std::u16string(kDeskName).append(u" (1)"));
  check_launch_template_desk_name(std::u16string(kDeskName).append(u" (3)"));

  // Same as above, but make sure that deleting the desk with the exact template
  // name still functions the same by only filling in whatever name is
  // available.
  RemoveDesk(desks_controller->desks()[1].get());
  check_launch_template_desk_name(kDeskName);
  check_launch_template_desk_name(std::u16string(kDeskName).append(u" (4)"));
}

// Tests that launching a template that contains a system web app works as
// expected.
IN_PROC_BROWSER_TEST_F(DesksClientTest, LaunchTemplateWithSystemApp) {
  ASSERT_TRUE(DesksClient::Get());

  // Create the settings app, which is a system web app.
  CreateSettingsSystemWebApp(browser()->profile());

  aura::Window* settings_window = FindBrowserWindow(kSettingsWindowId);
  ASSERT_TRUE(settings_window);
  const std::u16string settings_title = settings_window->GetTitle();

  std::unique_ptr<ash::DeskTemplate> desk_template =
      CaptureActiveDeskAndSaveTemplate();
  // Close the settings window. We'll need to verify if it reopens later.
  views::Widget* settings_widget =
      views::Widget::GetWidgetForNativeWindow(settings_window);
  settings_widget->CloseNow();
  ASSERT_FALSE(FindBrowserWindow(kSettingsWindowId));
  settings_window = nullptr;

  auto* desks_controller = ash::DesksController::Get();
  ASSERT_EQ(0, desks_controller->GetActiveDeskIndex());

  // Set the template we created as the template we want to launch.
  SetAndLaunchTemplate(std::move(desk_template));

  // Verify that the settings window has been launched on the new desk (desk B).
  // TODO(sammiequon): Right now the app just launches, so verify the title
  // matches. We should verify the restore id and use
  // `FindBrowserWindow(kSettingsWindowId)` once things are wired up properly.
  EXPECT_EQ(1, desks_controller->GetActiveDeskIndex());
  for (auto* browser : *BrowserList::GetInstance()) {
    aura::Window* window = browser->window()->GetNativeWindow();
    if (window->GetTitle() == settings_title) {
      settings_window = window;
      break;
    }
  }
  ASSERT_TRUE(settings_window);
  EXPECT_EQ(ash::Shell::GetContainer(settings_window->GetRootWindow(),
                                     ash::kShellWindowId_DeskContainerB),
            settings_window->parent());
}

// Tests that launching a template that contains a system web app will move the
// existing instance of the system web app to the current desk.
IN_PROC_BROWSER_TEST_F(DesksClientTest, LaunchTemplateWithSystemAppExisting) {
  ASSERT_TRUE(DesksClient::Get());
  Profile* profile = browser()->profile();

  // Create the settings app, which is a system web app.
  CreateSettingsSystemWebApp(profile);

  aura::Window* settings_window = FindBrowserWindow(kSettingsWindowId);
  ASSERT_TRUE(settings_window);
  EXPECT_EQ(2u, BrowserList::GetInstance()->size());

  // Give the settings app a known position.
  const gfx::Rect settings_bounds(100, 100, 600, 400);
  settings_window->SetBounds(settings_bounds);
  // Focus the browser so that the settings window is stacked at the bottom.
  browser()->window()->GetNativeWindow()->Focus();
  ASSERT_THAT(settings_window->parent()->children(),
              ElementsAre(settings_window, _));

  std::unique_ptr<ash::DeskTemplate> desk_template =
      CaptureActiveDeskAndSaveTemplate();

  // Move the settings window to a new place and stack it on top so that we can
  // later verify that it has been placed and stacked correctly.
  settings_window->SetBounds(gfx::Rect(150, 150, 650, 500));
  settings_window->Focus();

  ash::DesksController* desks_controller = ash::DesksController::Get();
  ASSERT_EQ(0, desks_controller->GetActiveDeskIndex());

  // Set the template we created as the template we want to launch.
  SetAndLaunchTemplate(std::move(desk_template));

  // We launch a new browser window, but not a new settings app. Verify that the
  // window has been moved to the right place and stacked at the bottom.
  EXPECT_EQ(3u, BrowserList::GetInstance()->size());
  EXPECT_TRUE(desks_controller->BelongsToActiveDesk(settings_window));
  EXPECT_EQ(settings_bounds, settings_window->bounds());
  ASSERT_THAT(settings_window->parent()->children(),
              ElementsAre(settings_window, _));
}

// Tests that launching a template that contains a chrome app works as expected.
IN_PROC_BROWSER_TEST_F(DesksClientTest, LaunchTemplateWithChromeApp) {
  DesksClient* desks_client = DesksClient::Get();
  ASSERT_TRUE(desks_client);

  // Create a chrome app.
  const extensions::Extension* extension =
      LoadAndLaunchPlatformApp("launch", "Launched");
  ASSERT_TRUE(extension);

  const std::string extension_id = extension->id();
  ::full_restore::SaveAppLaunchInfo(
      profile()->GetPath(),
      std::make_unique<app_restore::AppLaunchInfo>(
          extension_id, apps::mojom::LaunchContainer::kLaunchContainerWindow,
          WindowOpenDisposition::NEW_WINDOW, display::kDefaultDisplayId,
          std::vector<base::FilePath>{}, nullptr));

  extensions::AppWindow* app_window = CreateAppWindow(profile(), extension);
  ASSERT_TRUE(app_window);
  ASSERT_TRUE(GetFirstAppWindowForApp(extension_id));

  // Capture the active desk, which contains the chrome app.
  std::unique_ptr<ash::DeskTemplate> desk_template =
      CaptureActiveDeskAndSaveTemplate();
  ASSERT_TRUE(desk_template);

  // Close the chrome app window. We'll need to verify if it reopens later.
  views::Widget* app_widget =
      views::Widget::GetWidgetForNativeWindow(app_window->GetNativeWindow());
  app_widget->CloseNow();
  ASSERT_FALSE(GetFirstAppWindowForApp(extension_id));

  ash::DesksController* desks_controller = ash::DesksController::Get();
  ASSERT_EQ(0, desks_controller->GetActiveDeskIndex());

  // `BrowserAppLauncher::LaunchAppWithParams()` does not launch the chrome app
  // in tests, so here we set up a mock app launch handler and just verify a
  // `LaunchSystemWebAppOrChromeApp()` call with the associated extension is
  // seen.
  auto mock_app_launch_handler =
      std::make_unique<MockDeskTemplateAppLaunchHandler>(profile());
  MockDeskTemplateAppLaunchHandler* mock_app_launch_handler_ptr =
      mock_app_launch_handler.get();
  ScopedDeskClientAppLaunchHandlerSetter scoped_launch_handler(
      std::move(mock_app_launch_handler));

  EXPECT_CALL(*mock_app_launch_handler_ptr,
              LaunchSystemWebAppOrChromeApp(_, extension_id, _));

  // Set the template we created as the template we want to launch.
  SetAndLaunchTemplate(std::move(desk_template));
}

// Tests that launching a template that contains a browser window works as
// expected.
IN_PROC_BROWSER_TEST_F(DesksClientTest, LaunchTemplateWithBrowserWindow) {
  ASSERT_TRUE(DesksClient::Get());

  // Create a new browser and add a few tabs to it, and specify the active tab
  // index.
  const int browser_active_index = 1;
  Browser* browser = CreateBrowser(
      {GURL(kExampleUrl1), GURL(kExampleUrl2), GURL(kExampleUrl3)},
      /*active_url_index=*/browser_active_index);

  // Verify that the active tab is correct.
  EXPECT_EQ(browser_active_index, browser->tab_strip_model()->active_index());

  aura::Window* window = browser->window()->GetNativeWindow();
  const int32_t browser_window_id =
      window->GetProperty(app_restore::kWindowIdKey);
  // Get current tabs from browser.
  const std::vector<GURL> urls = GetURLsForBrowserWindow(browser);

  std::unique_ptr<ash::DeskTemplate> desk_template =
      CaptureActiveDeskAndSaveTemplate();
  ASSERT_TRUE(desk_template);

  ash::DesksController* desks_controller = ash::DesksController::Get();
  ASSERT_EQ(0, desks_controller->GetActiveDeskIndex());

  // Set the template we created as the template we want to launch.
  SetAndLaunchTemplate(std::move(desk_template));

  // Verify that the browser was launched with the correct urls and active tab.
  Browser* new_browser = FindBrowser(browser_window_id);
  ASSERT_TRUE(new_browser);
  EXPECT_EQ(urls, GetURLsForBrowserWindow(new_browser));
  EXPECT_EQ(browser_active_index,
            new_browser->tab_strip_model()->active_index());

  // Verify that the browser window has been launched on the new desk (desk B).
  EXPECT_EQ(1, desks_controller->GetActiveDeskIndex());
  aura::Window* browser_window = new_browser->window()->GetNativeWindow();
  ASSERT_TRUE(browser_window);
  EXPECT_EQ(ash::Shell::GetContainer(browser_window->GetRootWindow(),
                                     ash::kShellWindowId_DeskContainerB),
            browser_window->parent());
}

// Tests that browser session restore isn't triggered when we launch a template
// that contains a browser window.
IN_PROC_BROWSER_TEST_F(DesksClientTest, PreventBrowserSessionRestoreTest) {
  ASSERT_TRUE(DesksClient::Get());

  // Do not exit from test or delete the Profile* when last browser is closed.
  ScopedKeepAlive keep_alive(KeepAliveOrigin::BROWSER,
                             KeepAliveRestartOption::DISABLED);
  ScopedProfileKeepAlive profile_keep_alive(
      browser()->profile(), ProfileKeepAliveOrigin::kBrowserWindow);

  // Enable session service.
  SessionStartupPref pref(SessionStartupPref::LAST);
  Profile* profile = browser()->profile();
  SessionStartupPref::SetStartupPref(profile, pref);

  const int expected_tab_count = 2;
  chrome::AddTabAt(browser(), GURL(kExampleUrl2), /*index=*/-1,
                   /*foreground=*/true);
  EXPECT_EQ(expected_tab_count, browser()->tab_strip_model()->count());
  const int32_t browser_window_id =
      browser()->window()->GetNativeWindow()->GetProperty(
          app_restore::kWindowIdKey);

  std::unique_ptr<ash::DeskTemplate> desk_template =
      CaptureActiveDeskAndSaveTemplate();
  ASSERT_TRUE(desk_template);

  // Close the browser and verify that all browser windows are closed.
  CloseBrowserSynchronously(browser());
  EXPECT_EQ(0u, chrome::GetTotalBrowserCount());

  // Set the template we created and launch the template.
  SetAndLaunchTemplate(std::move(desk_template));

  // Verify that the browser was launched with the correct number of tabs, and
  // that browser session restore did not restore any windows/tabs.
  Browser* new_browser = FindBrowser(browser_window_id);
  ASSERT_TRUE(new_browser);
  EXPECT_EQ(expected_tab_count, GetURLsForBrowserWindow(new_browser).size());
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
}

// Tests that the windows and tabs count histogram is recorded properly.
IN_PROC_BROWSER_TEST_F(DesksClientTest,
                       DeskTemplateWindowAndTabCountHistogram) {
  ASSERT_TRUE(DesksClient::Get());

  base::HistogramTester histogram_tester;

  Profile* profile = browser()->profile();

  // Create the settings app, which is a system web app.
  CreateSettingsSystemWebApp(profile);

  CreateBrowser({GURL(kExampleUrl1), GURL(kExampleUrl2)});
  CreateBrowser({GURL(kExampleUrl1), GURL(kExampleUrl2), GURL(kExampleUrl3)});

  std::unique_ptr<ash::DeskTemplate> desk_template =
      CaptureActiveDeskAndSaveTemplate();
  ASSERT_TRUE(desk_template);

  const app_restore::RestoreData* restore_data =
      desk_template->desk_restore_data();
  const auto& app_id_to_launch_list = restore_data->app_id_to_launch_list();
  EXPECT_EQ(app_id_to_launch_list.size(), 2u);

  constexpr char kWindowCountHistogramName[] = "Ash.DeskTemplate.WindowCount";
  constexpr char kTabCountHistogramName[] = "Ash.DeskTemplate.TabCount";
  constexpr char kWindowAndTabCountHistogramName[] =
      "Ash.DeskTemplate.WindowAndTabCount";
  // NOTE: there is an existing browser with 1 tab created by BrowserMain().
  histogram_tester.ExpectBucketCount(kWindowCountHistogramName, 4, 1);
  histogram_tester.ExpectBucketCount(kTabCountHistogramName, 6, 1);
  histogram_tester.ExpectBucketCount(kWindowAndTabCountHistogramName, 7, 1);
}

// Tests that the launch from template histogram is recorded properly.
IN_PROC_BROWSER_TEST_F(DesksClientTest,
                       DeskTemplateLaunchFromTemplateHistogram) {
  ASSERT_TRUE(DesksClient::Get());

  base::HistogramTester histogram_tester;

  // Create a new browser.
  CreateBrowser({});

  // Save the template.
  std::unique_ptr<ash::DeskTemplate> desk_template =
      CaptureActiveDeskAndSaveTemplate();
  ASSERT_TRUE(desk_template);

  // Set the template we created as the template we want to launch.
  ash::DeskTemplate* desk_template_ptr = desk_template.get();
  SetTemplate(std::move(desk_template));

  int launches = 5;
  for (int i = 0; i < launches; i++)
    LaunchTemplate(desk_template_ptr->uuid());

  constexpr char kLaunchFromTemplateHistogramName[] =
      "Ash.DeskTemplate.LaunchFromTemplate";
  histogram_tester.ExpectTotalCount(kLaunchFromTemplateHistogramName, launches);
}

// Tests that the template count histogram is recorded properly.
IN_PROC_BROWSER_TEST_F(DesksClientTest,
                       DeskTemplateUserTemplateCountHistogram) {
  ASSERT_TRUE(DesksClient::Get());

  base::HistogramTester histogram_tester;

  // Verify that all template saves and deletes are captured by the histogram.
  CaptureActiveDeskAndSaveTemplate();
  CaptureActiveDeskAndSaveTemplate();
  std::unique_ptr<ash::DeskTemplate> desk_template =
      CaptureActiveDeskAndSaveTemplate();
  DeleteDeskTemplate(desk_template->uuid());
  CaptureActiveDeskAndSaveTemplate();

  constexpr char kUserTemplateCountHistogramName[] =
      "Ash.DeskTemplate.UserTemplateCount";
  histogram_tester.ExpectBucketCount(kUserTemplateCountHistogramName, 1, 1);
  histogram_tester.ExpectBucketCount(kUserTemplateCountHistogramName, 2, 2);
  histogram_tester.ExpectBucketCount(kUserTemplateCountHistogramName, 3, 2);
}

// Tests that browser windows created from a template have the correct bounds
// and window state.
IN_PROC_BROWSER_TEST_F(DesksClientTest, BrowserWindowRestorationTest) {
  ASSERT_TRUE(DesksClient::Get());

  // Create a new browser and set its bounds.
  Browser* browser_1 = CreateBrowser({GURL(kExampleUrl1), GURL(kExampleUrl2)});
  const gfx::Rect browser_bounds_1 = gfx::Rect(100, 100, 600, 200);
  aura::Window* window_1 = browser_1->window()->GetNativeWindow();
  window_1->SetBounds(browser_bounds_1);

  // Create a new minimized browser.
  Browser* browser_2 = CreateBrowser({GURL(kExampleUrl1)});
  const gfx::Rect browser_bounds_2 = gfx::Rect(150, 150, 500, 300);
  aura::Window* window_2 = browser_2->window()->GetNativeWindow();
  window_2->SetBounds(browser_bounds_2);
  EXPECT_EQ(browser_bounds_2, window_2->bounds());
  browser_2->window()->Minimize();

  // Create a new maximized browser.
  Browser* browser_3 = CreateBrowser({GURL(kExampleUrl1)});
  browser_3->window()->Maximize();

  EXPECT_EQ(browser_bounds_1, window_1->bounds());
  EXPECT_EQ(browser_bounds_2, window_2->bounds());
  ASSERT_TRUE(browser_2->window()->IsMinimized());
  ASSERT_TRUE(browser_3->window()->IsMaximized());

  const int32_t browser_window_id_1 =
      window_1->GetProperty(app_restore::kWindowIdKey);
  const int32_t browser_window_id_2 =
      window_2->GetProperty(app_restore::kWindowIdKey);
  const int32_t browser_window_id_3 =
      browser_3->window()->GetNativeWindow()->GetProperty(
          app_restore::kWindowIdKey);

  // Capture the active desk, which contains the two browser windows.
  std::unique_ptr<ash::DeskTemplate> desk_template =
      CaptureActiveDeskAndSaveTemplate();

  // Set the template and launch it.
  SetAndLaunchTemplate(std::move(desk_template));

  // Verify that the browser was launched with the correct bounds.
  Browser* new_browser_1 = FindBrowser(browser_window_id_1);
  ASSERT_TRUE(new_browser_1);
  EXPECT_EQ(browser_bounds_1,
            new_browser_1->window()->GetNativeWindow()->bounds());

  // Verify that the browser was launched and minimized.
  Browser* new_browser_2 = FindBrowser(browser_window_id_2);
  ASSERT_TRUE(new_browser_2);
  ASSERT_TRUE(new_browser_2->window()->IsMinimized());
  EXPECT_EQ(browser_bounds_2,
            new_browser_2->window()->GetNativeWindow()->bounds());

  // Verify that the browser was launched and maximized.
  Browser* new_browser_3 = FindBrowser(browser_window_id_3);
  ASSERT_TRUE(new_browser_3);
  ASSERT_TRUE(new_browser_3->window()->IsMaximized());
}

// Tests that saving and launching a template that contains a PWA works as
// expected.
IN_PROC_BROWSER_TEST_F(DesksClientTest, LaunchTemplateWithPWA) {
  ASSERT_TRUE(DesksClient::Get());

  Browser* pwa_browser =
      InstallAndLaunchPWA(GURL(kExampleUrl1), /*launch_in_browser=*/false);
  ASSERT_TRUE(pwa_browser->is_type_app());
  aura::Window* pwa_window = pwa_browser->window()->GetNativeWindow();
  const gfx::Rect pwa_bounds(50, 50, 500, 500);
  pwa_window->SetBounds(pwa_bounds);
  const int32_t pwa_window_id =
      pwa_window->GetProperty(app_restore::kWindowIdKey);
  const std::string* app_name =
      pwa_window->GetProperty(app_restore::kBrowserAppNameKey);
  ASSERT_TRUE(app_name);

  // Capture the active desk, which contains the PWA.
  std::unique_ptr<ash::DeskTemplate> desk_template =
      CaptureActiveDeskAndSaveTemplate();

  // Find |pwa_browser| window's app restore data.
  const app_restore::RestoreData* restore_data =
      desk_template->desk_restore_data();
  const auto& app_id_to_launch_list = restore_data->app_id_to_launch_list();
  EXPECT_EQ(app_id_to_launch_list.size(), 1u);
  ASSERT_TRUE(restore_data->HasAppTypeBrowser());
  auto iter = app_id_to_launch_list.find(extension_misc::kChromeAppId);
  ASSERT_TRUE(iter != app_id_to_launch_list.end());
  auto app_restore_data_iter = iter->second.find(pwa_window_id);
  ASSERT_TRUE(app_restore_data_iter != iter->second.end());
  const auto& data = app_restore_data_iter->second;
  // Verify window info are correctly captured.
  EXPECT_EQ(pwa_bounds, data->current_bounds.value());
  ASSERT_TRUE(data->app_type_browser.has_value() &&
              data->app_type_browser.value());
  EXPECT_EQ(*app_name, *data->app_name);

  // Set the template and launch it.
  SetAndLaunchTemplate(std::move(desk_template));

  // Verify that the PWA was launched correctly.
  Browser* new_pwa_browser = FindBrowser(pwa_window_id);
  ASSERT_TRUE(new_pwa_browser);
  ASSERT_TRUE(new_pwa_browser->is_type_app());
  aura::Window* new_browser_window =
      new_pwa_browser->window()->GetNativeWindow();
  EXPECT_NE(new_browser_window, pwa_window);
  EXPECT_EQ(pwa_bounds, new_browser_window->bounds());
  const std::string* new_app_name =
      new_browser_window->GetProperty(app_restore::kBrowserAppNameKey);
  ASSERT_TRUE(new_app_name);
  EXPECT_EQ(*app_name, *new_app_name);
}

// Tests that saving and launching a template that contains a PWA in a browser
// window works as expected.
IN_PROC_BROWSER_TEST_F(DesksClientTest, LaunchTemplateWithPWAInBrowser) {
  ASSERT_TRUE(DesksClient::Get());

  Browser* pwa_browser =
      InstallAndLaunchPWA(GURL(kYoutubeUrl), /*launch_in_browser=*/true);
  aura::Window* pwa_window = pwa_browser->window()->GetNativeWindow();
  const int32_t pwa_window_id =
      pwa_window->GetProperty(app_restore::kWindowIdKey);

  // Capture the active desk, which contains the PWA.
  std::unique_ptr<ash::DeskTemplate> desk_template =
      CaptureActiveDeskAndSaveTemplate();

  // Test that |pwa_browser| restore data can be found.
  const app_restore::RestoreData* restore_data =
      desk_template->desk_restore_data();
  const auto& app_id_to_launch_list = restore_data->app_id_to_launch_list();
  EXPECT_EQ(app_id_to_launch_list.size(), 1u);

  // Test that |pwa_browser|'s restore data is saved under the Chrome browser
  // app id extension_misc::kChromeAppId, not Youtube app id
  // extension_misc::kYoutubeAppId.
  auto iter = app_id_to_launch_list.find(extension_misc::kChromeAppId);
  ASSERT_TRUE(iter != app_id_to_launch_list.end());
  auto app_restore_data_iter = iter->second.find(pwa_window_id);
  ASSERT_TRUE(app_restore_data_iter != iter->second.end());

  iter = app_id_to_launch_list.find(extension_misc::kYoutubeAppId);
  EXPECT_FALSE(iter != app_id_to_launch_list.end());
}

class DesksClientMultiProfileTest : public ash::LoginManagerTest {
 public:
  DesksClientMultiProfileTest() : ash::LoginManagerTest() {
    login_mixin_.AppendRegularUsers(2);
    account_id1_ = login_mixin_.users()[0].account_id;
    account_id2_ = login_mixin_.users()[1].account_id;

    // This feature depends on full restore feature, so need to enable it.
    scoped_feature_list_.InitAndEnableFeature(
        full_restore::features::kFullRestore);
  }
  ~DesksClientMultiProfileTest() override = default;

  void SetUpOnMainThread() override {
    ash::LoginManagerTest::SetUpOnMainThread();

    LoginUser(account_id1_);
    ::full_restore::SetActiveProfilePath(
        chromeos::ProfileHelper::Get()
            ->GetProfileByAccountId(account_id1_)
            ->GetPath());
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  ash::LoginManagerMixin login_mixin_{&mixin_host_};
  AccountId account_id1_;
  AccountId account_id2_;
};

IN_PROC_BROWSER_TEST_F(DesksClientMultiProfileTest, MultiProfileTest) {
  CreateBrowser(
      chromeos::ProfileHelper::Get()->GetProfileByAccountId(account_id1_));
  // Capture the active desk, which contains the browser windows.
  std::unique_ptr<ash::DeskTemplate> desk_template =
      CaptureActiveDeskAndSaveTemplate();
  const app_restore::RestoreData* restore_data =
      desk_template->desk_restore_data();
  const auto& app_id_to_launch_list = restore_data->app_id_to_launch_list();
  EXPECT_EQ(app_id_to_launch_list.size(), 1u);

  auto get_templates_size = []() {
    base::RunLoop run_loop;
    int templates_num = 0;
    DesksClient::Get()->GetDeskTemplates(base::BindLambdaForTesting(
        [&](const std::vector<ash::DeskTemplate*>& desk_templates,
            std::string error_string) {
          templates_num = desk_templates.size();
          run_loop.Quit();
        }));
    run_loop.Run();
    return templates_num;
  };
  EXPECT_EQ(get_templates_size(), 1);

  // Now switch to |account_id2_|. Test that the captured desk template can't
  // be accessed from |account_id2_|.
  ash::UserAddingScreen::Get()->Start();
  AddUser(account_id2_);
  EXPECT_EQ(get_templates_size(), 0);
}
