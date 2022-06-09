// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_manager.h"

#include <map>

#include "ash/components/settings/cros_settings_names.h"
#include "base/bind.h"
#include "chrome/browser/ash/app_mode/app_session_ash.h"
#include "chrome/browser/ash/app_mode/kiosk_cryptohome_remover.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/policy/core/device_local_account.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_application_info.h"
#include "components/prefs/pref_registry_simple.h"

namespace ash {

namespace {
// This class is owned by `ChromeBrowserMainPartsAsh`.
static WebKioskAppManager* g_web_kiosk_app_manager = nullptr;
}  // namespace

// static
const char WebKioskAppManager::kWebKioskDictionaryName[] = "web-kiosk";

// static
void WebKioskAppManager::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kWebKioskDictionaryName);
}

// static
bool WebKioskAppManager::IsInitialized() {
  return g_web_kiosk_app_manager;
}

// static
WebKioskAppManager* WebKioskAppManager::Get() {
  CHECK(g_web_kiosk_app_manager);
  return g_web_kiosk_app_manager;
}

// static
KioskAppManagerBase::App WebKioskAppManager::CreateAppByData(
    const WebKioskAppData& data) {
  auto app = KioskAppManagerBase::App(data);
  app.url = data.install_url();
  return app;
}

WebKioskAppManager::WebKioskAppManager()
    : auto_launch_account_id_(EmptyAccountId()) {
  CHECK(!g_web_kiosk_app_manager);  // Only one instance is allowed.
  g_web_kiosk_app_manager = this;
  UpdateAppsFromPolicy();
}

WebKioskAppManager::~WebKioskAppManager() {
  g_web_kiosk_app_manager = nullptr;
}

void WebKioskAppManager::GetApps(std::vector<App>* apps) const {
  apps->clear();
  apps->reserve(apps_.size());
  for (auto& web_app : apps_) {
    App app(*web_app);
    app.url = web_app->install_url();
    apps->push_back(std::move(app));
  }
}

void WebKioskAppManager::LoadIcons() {
  for (auto& web_app : apps_)
    web_app->LoadIcon();
}

const AccountId& WebKioskAppManager::GetAutoLaunchAccountId() const {
  return auto_launch_account_id_;
}

const WebKioskAppData* WebKioskAppManager::GetAppByAccountId(
    const AccountId& account_id) const {
  for (const auto& web_app : apps_) {
    if (web_app->account_id() == account_id) {
      return web_app.get();
    }
  }
  return nullptr;
}

void WebKioskAppManager::UpdateAppByAccountId(
    const AccountId& account_id,
    std::unique_ptr<WebApplicationInfo> app_info) {
  for (auto& web_app : apps_) {
    if (web_app->account_id() == account_id) {
      web_app->UpdateFromWebAppInfo(std::move(app_info));
      return;
    }
  }
  NOTREACHED();
}

void WebKioskAppManager::AddAppForTesting(const AccountId& account_id,
                                          const GURL& install_url) {
  const std::string app_id =
      web_app::GenerateAppId(/*manifest_id=*/absl::nullopt, install_url);
  apps_.push_back(std::make_unique<WebKioskAppData>(
      this, app_id, account_id, install_url, /*title*/ std::string(),
      /*icon_url*/ GURL()));
  NotifyKioskAppsChanged();
}

void WebKioskAppManager::InitSession(Browser* browser, Profile* profile) {
  LOG_IF(FATAL, app_session_) << "Kiosk session is already initialized.";

  app_session_ = std::make_unique<AppSessionAsh>();
  if (crosapi::browser_util::IsLacrosEnabledInWebKioskSession())
    app_session_->InitForWebKioskWithLacros(profile);
  else
    app_session_->InitForWebKiosk(browser);

  NotifySessionInitialized();
}

void WebKioskAppManager::UpdateAppsFromPolicy() {
  // Store current apps. We will compare old and new apps to determine which
  // apps are new, and which were deleted.
  std::map<std::string, std::unique_ptr<WebKioskAppData>> old_apps;
  for (auto& app : apps_)
    old_apps[app->app_id()] = std::move(app);
  apps_.clear();
  auto_launch_account_id_.clear();
  auto_launched_with_zero_delay_ = false;
  std::string auto_login_account_id_from_settings;
  CrosSettings::Get()->GetString(kAccountsPrefDeviceLocalAccountAutoLoginId,
                                 &auto_login_account_id_from_settings);

  // Re-populates |apps_| and reuses existing apps when possible.
  const std::vector<policy::DeviceLocalAccount> device_local_accounts =
      policy::GetDeviceLocalAccounts(CrosSettings::Get());
  for (auto account : device_local_accounts) {
    if (account.type != policy::DeviceLocalAccount::TYPE_WEB_KIOSK_APP)
      continue;
    const AccountId account_id(AccountId::FromUserEmail(account.user_id));

    if (account.account_id == auto_login_account_id_from_settings) {
      auto_launch_account_id_ = account_id;
      int auto_launch_delay = 0;
      CrosSettings::Get()->GetInteger(
          kAccountsPrefDeviceLocalAccountAutoLoginDelay, &auto_launch_delay);
      auto_launched_with_zero_delay_ = auto_launch_delay == 0;
    }

    GURL url(account.web_kiosk_app_info.url());
    std::string title = account.web_kiosk_app_info.title();
    GURL icon_url = GURL(account.web_kiosk_app_info.icon_url());

    std::string app_id =
        web_app::GenerateAppId(/*manifest_id=*/absl::nullopt, url);

    auto old_it = old_apps.find(app_id);
    if (old_it != old_apps.end()) {
      apps_.push_back(std::move(old_it->second));
      old_apps.erase(old_it);
    } else {
      apps_.push_back(std::make_unique<WebKioskAppData>(
          this, app_id, account_id, std::move(url), title,
          std::move(icon_url)));
      apps_.back()->LoadFromCache();
    }

    KioskCryptohomeRemover::CancelDelayedCryptohomeRemoval(account_id);
  }

  std::vector<KioskAppDataBase*> old_apps_to_remove;
  for (auto& entry : old_apps)
    old_apps_to_remove.emplace_back(entry.second.get());
  ClearRemovedApps(old_apps_to_remove);
  NotifyKioskAppsChanged();
}

}  // namespace ash
