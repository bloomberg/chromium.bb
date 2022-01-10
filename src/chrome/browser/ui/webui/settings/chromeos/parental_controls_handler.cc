// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/parental_controls_handler.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/values.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/ash/child_accounts/child_user_service.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/webui/chromeos/add_supervision/add_supervision_ui.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/display/types/display_constants.h"
#include "ui/events/event_constants.h"
#include "url/gurl.h"

namespace chromeos {
namespace settings {

const char kFamilyLinkSiteURL[] = "https://families.google.com/families";

ParentalControlsHandler::ParentalControlsHandler(Profile* profile)
    : profile_(profile) {}

ParentalControlsHandler::~ParentalControlsHandler() = default;

void ParentalControlsHandler::RegisterMessages() {
  web_ui()->RegisterDeprecatedMessageCallback(
      "showAddSupervisionDialog",
      base::BindRepeating(
          &ParentalControlsHandler::HandleShowAddSupervisionDialog,
          base::Unretained(this)));

  web_ui()->RegisterDeprecatedMessageCallback(
      "launchFamilyLinkSettings",
      base::BindRepeating(
          &ParentalControlsHandler::HandleLaunchFamilyLinkSettings,
          base::Unretained(this)));
}

void ParentalControlsHandler::OnJavascriptAllowed() {}
void ParentalControlsHandler::OnJavascriptDisallowed() {}

void ParentalControlsHandler::HandleShowAddSupervisionDialog(
    const base::ListValue* args) {
  DCHECK(args->GetList().empty());
  AddSupervisionDialog::Show();
}

void ParentalControlsHandler::HandleLaunchFamilyLinkSettings(
    const base::ListValue* args) {
  DCHECK(args->GetList().empty());

  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile_);

  apps::AppRegistryCache& registry = proxy->AppRegistryCache();
  const std::string app_id = arc::ArcPackageNameToAppId(
      chromeos::ChildUserService::kFamilyLinkHelperAppPackageName, profile_);
  if (registry.GetAppType(app_id) != apps::mojom::AppType::kUnknown) {
    // Launch FLH app since it is available.
    proxy->Launch(app_id, ui::EventFlags::EF_NONE,
                  apps::mojom::LaunchSource::kFromParentalControls,
                  apps::MakeWindowInfo(display::kDefaultDisplayId));
    return;
  }

  // No FLH app installed, so try to launch Play Store to FLH app install page.
  if (registry.GetAppType(arc::kPlayStoreAppId) !=
      apps::mojom::AppType::kUnknown) {
    proxy->LaunchAppWithUrl(
        arc::kPlayStoreAppId, ui::EF_NONE,
        GURL(chromeos::ChildUserService::kFamilyLinkHelperAppPlayStoreURL),
        apps::mojom::LaunchSource::kFromChromeInternal);
    return;
  }

  // As a last resort, launch browser to the family link site.
  NavigateParams params(profile_, GURL(kFamilyLinkSiteURL),
                        ui::PAGE_TRANSITION_FROM_API);
  params.disposition = WindowOpenDisposition::NEW_WINDOW;
  params.window_action = NavigateParams::SHOW_WINDOW;
  Navigate(&params);
}

}  // namespace settings
}  // namespace chromeos
