// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/web_app_dialog_utils.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/browser/web_applications/web_application_info.h"
#include "components/webapps/browser/banners/app_banner_manager.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/browser/navigation_entry.h"

namespace web_app {

namespace {

void OnWebAppInstallShowInstallDialog(
    webapps::WebappInstallSource install_source,
    chrome::PwaInProductHelpState iph_state,
    content::WebContents* initiator_web_contents,
    std::unique_ptr<WebApplicationInfo> web_app_info,
    ForInstallableSite for_installable_site,
    WebAppInstallationAcceptanceCallback web_app_acceptance_callback) {
  DCHECK(web_app_info);
  if (for_installable_site == ForInstallableSite::kYes) {
    web_app_info->user_display_mode = blink::mojom::DisplayMode::kStandalone;
    chrome::ShowPWAInstallBubble(
        initiator_web_contents, std::move(web_app_info),
        std::move(web_app_acceptance_callback), iph_state);
  } else {
    chrome::ShowWebAppInstallDialog(initiator_web_contents,
                                    std::move(web_app_info),
                                    std::move(web_app_acceptance_callback));
  }
}

WebAppInstalledCallback& GetInstalledCallbackForTesting() {
  static base::NoDestructor<WebAppInstalledCallback> instance;
  return *instance;
}

void OnWebAppInstalled(WebAppInstalledCallback callback,
                       const AppId& installed_app_id,
                       InstallResultCode code) {
  if (GetInstalledCallbackForTesting())
    std::move(GetInstalledCallbackForTesting()).Run(installed_app_id, code);

  std::move(callback).Run(installed_app_id, code);
}

}  // namespace

bool CanCreateWebApp(const Browser* browser) {
  // Check whether user is allowed to install web app.
  if (!WebAppProvider::GetForWebApps(browser->profile()) ||
      !AreWebAppsUserInstallable(browser->profile()))
    return false;

  // Check whether we're able to install the current page as an app.
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  if (!IsValidWebAppUrl(web_contents->GetLastCommittedURL()) ||
      web_contents->IsCrashed())
    return false;
  content::NavigationEntry* entry =
      web_contents->GetController().GetLastCommittedEntry();
  if (entry && entry->GetPageType() == content::PAGE_TYPE_ERROR)
    return false;

  return true;
}

bool CanPopOutWebApp(Profile* profile) {
  return AreWebAppsEnabled(profile) && !profile->IsGuestSession() &&
         !profile->IsOffTheRecord();
}

void CreateWebAppFromCurrentWebContents(Browser* browser,
                                        bool force_shortcut_app) {
  DCHECK(CanCreateWebApp(browser));

  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  auto* provider = WebAppProvider::GetForWebContents(web_contents);
  DCHECK(provider);

  if (provider->install_manager().IsInstallingForWebContents(web_contents))
    return;

  webapps::WebappInstallSource install_source =
      webapps::InstallableMetrics::GetInstallSource(
          web_contents, force_shortcut_app
                            ? webapps::InstallTrigger::CREATE_SHORTCUT
                            : webapps::InstallTrigger::MENU);

  WebAppInstalledCallback callback = base::DoNothing();

  provider->install_manager().InstallWebAppFromManifestWithFallback(
      web_contents, force_shortcut_app, install_source,
      base::BindOnce(OnWebAppInstallShowInstallDialog, install_source,
                     chrome::PwaInProductHelpState::kNotShown),
      base::BindOnce(OnWebAppInstalled, std::move(callback)));
}

bool CreateWebAppFromManifest(content::WebContents* web_contents,
                              bool bypass_service_worker_check,
                              webapps::WebappInstallSource install_source,
                              WebAppInstalledCallback installed_callback,
                              chrome::PwaInProductHelpState iph_state) {
  auto* provider = WebAppProvider::GetForWebContents(web_contents);
  if (!provider)
    return false;

  provider->install_manager().InstallWebAppFromManifest(
      web_contents, bypass_service_worker_check, install_source,
      base::BindOnce(OnWebAppInstallShowInstallDialog, install_source,
                     iph_state),
      base::BindOnce(OnWebAppInstalled, std::move(installed_callback)));
  return true;
}

void SetInstalledCallbackForTesting(WebAppInstalledCallback callback) {
  GetInstalledCallbackForTesting() = std::move(callback);
}

}  // namespace web_app
