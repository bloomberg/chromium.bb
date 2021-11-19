// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/android_apps_handler.h"

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"  // kSettingsAppId
#include "components/services/app_service/public/cpp/intent_util.h"
#include "content/public/browser/web_contents.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/event_constants.h"

namespace chromeos {
namespace settings {

AndroidAppsHandler::AndroidAppsHandler(Profile* profile,
                                       apps::AppServiceProxy* app_service_proxy)
    : profile_(profile), app_service_proxy_(app_service_proxy) {}

AndroidAppsHandler::~AndroidAppsHandler() {}

void AndroidAppsHandler::RegisterMessages() {
  // Note: requestAndroidAppsInfo must be called before observers will be added.
  web_ui()->RegisterDeprecatedMessageCallback(
      "requestAndroidAppsInfo",
      base::BindRepeating(&AndroidAppsHandler::HandleRequestAndroidAppsInfo,
                          weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterDeprecatedMessageCallback(
      "showAndroidAppsSettings",
      base::BindRepeating(&AndroidAppsHandler::ShowAndroidAppsSettings,
                          weak_ptr_factory_.GetWeakPtr()));
}

void AndroidAppsHandler::OnJavascriptAllowed() {
  ArcAppListPrefs* arc_prefs = ArcAppListPrefs::Get(profile_);
  if (arc_prefs) {
    arc_prefs_observation_.Observe(arc_prefs);
    // arc::ArcSessionManager is associated with primary profile.
    arc_session_manager_observation_.Observe(arc::ArcSessionManager::Get());
  }
}

void AndroidAppsHandler::OnJavascriptDisallowed() {
  arc_prefs_observation_.Reset();
  arc_session_manager_observation_.Reset();
}

void AndroidAppsHandler::OnAppRegistered(
    const std::string& app_id,
    const ArcAppListPrefs::AppInfo& app_info) {
  HandleAppChanged(app_id);
}

void AndroidAppsHandler::OnAppStatesChanged(
    const std::string& app_id,
    const ArcAppListPrefs::AppInfo& app_info) {
  HandleAppChanged(app_id);
}

void AndroidAppsHandler::OnAppRemoved(const std::string& app_id) {
  HandleAppChanged(app_id);
}

void AndroidAppsHandler::HandleAppChanged(const std::string& app_id) {
  if (app_id != arc::kSettingsAppId)
    return;
  SendAndroidAppsInfo();
}

void AndroidAppsHandler::OnArcPlayStoreEnabledChanged(bool enabled) {
  SendAndroidAppsInfo();
}

std::unique_ptr<base::DictionaryValue>
AndroidAppsHandler::BuildAndroidAppsInfo() {
  std::unique_ptr<base::DictionaryValue> info(new base::DictionaryValue);
  info->SetBoolean("playStoreEnabled",
                   arc::IsArcPlayStoreEnabledForProfile(profile_));
  const ArcAppListPrefs* arc_apps_pref = ArcAppListPrefs::Get(profile_);
  // TODO(khmel): Inverstigate why in some browser tests
  // playStoreEnabled is true but arc_apps_pref is not set.
  info->SetBoolean(
      "settingsAppAvailable",
      arc_apps_pref && arc_apps_pref->IsRegistered(arc::kSettingsAppId));
  return info;
}

void AndroidAppsHandler::HandleRequestAndroidAppsInfo(
    const base::ListValue* args) {
  AllowJavascript();
  SendAndroidAppsInfo();
}

void AndroidAppsHandler::SendAndroidAppsInfo() {
  std::unique_ptr<base::DictionaryValue> info = BuildAndroidAppsInfo();
  FireWebUIListener("android-apps-info-update", *info);
}

void AndroidAppsHandler::ShowAndroidAppsSettings(const base::ListValue* args) {
  CHECK_EQ(1U, args->GetList().size());
  bool activated_from_keyboard = false;
  if (args->GetList()[0].is_bool())
    activated_from_keyboard = args->GetList()[0].GetBool();
  int flags = activated_from_keyboard ? ui::EF_NONE : ui::EF_LEFT_MOUSE_BUTTON;

  app_service_proxy_->Launch(
      arc::kSettingsAppId, flags,
      apps::mojom::LaunchSource::kFromParentalControls,
      apps::MakeWindowInfo(GetDisplayIdForCurrentProfile()));
}

int64_t AndroidAppsHandler::GetDisplayIdForCurrentProfile() {
  // Settings in secondary profile cannot access ARC.
  DCHECK(arc::IsArcAllowedForProfile(profile_));
  return display::Screen::GetScreen()
      ->GetDisplayNearestView(web_ui()->GetWebContents()->GetNativeView())
      .id();
}

}  // namespace settings
}  // namespace chromeos
