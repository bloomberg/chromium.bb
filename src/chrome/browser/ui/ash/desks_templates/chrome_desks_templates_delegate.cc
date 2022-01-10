// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/desks_templates/chrome_desks_templates_delegate.h"

#include "ash/constants/app_types.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/desk_template.h"
#include "ash/public/cpp/toast_data.h"
#include "ash/public/cpp/toast_manager.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/bind.h"
#include "base/check.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/desks_templates/desks_templates_client.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/theme_resources.h"
#include "components/app_restore/app_launch_info.h"
#include "components/app_restore/app_restore_data.h"
#include "components/app_restore/full_restore_save_handler.h"
#include "components/app_restore/full_restore_utils.h"
#include "components/app_restore/restore_data.h"
#include "components/app_restore/window_properties.h"
#include "components/favicon/core/favicon_service.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/types_util.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "components/user_manager/user_manager.h"
#include "extensions/common/constants.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/native_theme/themed_vector_icon.h"
#include "url/gurl.h"

namespace {

// Name for app not available toast.
constexpr char kAppNotAvailableTemplateToastName[] =
    "AppNotAvailableTemplateToast";

// The amount of time for which the launch template toasts will remain
// displayed.
constexpr int kLaunchTemplateToastDurationMs = 6 * 1000;

// Returns the TabStripModel that associates with `window` if the given `window`
// contains a browser frame, otherwise returns nullptr.
TabStripModel* GetTabstripModelForWindowIfAny(aura::Window* window) {
  BrowserView* browser_view =
      BrowserView::GetBrowserViewForNativeWindow(window);
  return browser_view ? browser_view->browser()->tab_strip_model() : nullptr;
}

// Returns the list of URLs that are open in `tab_strip_model`.
std::vector<GURL> GetURLsIfApplicable(TabStripModel* tab_strip_model) {
  DCHECK(tab_strip_model);

  std::vector<GURL> urls;
  for (int i = 0; i < tab_strip_model->count(); ++i)
    urls.push_back(tab_strip_model->GetWebContentsAt(i)->GetLastCommittedURL());
  return urls;
}

// Return true if `app_id` is available to launch from template.
bool IsAppAvailable(const std::string& app_id,
                    apps::AppServiceProxy* app_service_proxy) {
  DCHECK(app_service_proxy);
  bool installed = false;
  Profile* app_profile = ProfileManager::GetActiveUserProfile();
  DCHECK(app_profile);

  app_service_proxy->AppRegistryCache().ForOneApp(
      app_id, [&](const apps::AppUpdate& app) {
        installed = apps_util::IsInstalled(app.Readiness());
      });
  if (installed)
    return true;
  const extensions::Extension* app =
      extensions::ExtensionRegistry::Get(app_profile)
          ->GetInstalledExtension(app_id);
  return app != nullptr;
}

// Check app availability from `launch_list`, return a vector of unavailable app
// names.
std::vector<std::string> GetUnavailableAppNames(
    const app_restore::RestoreData::AppIdToLaunchList& launch_list) {
  std::vector<std::string> app_names;
  auto* app_service_proxy = apps::AppServiceProxyFactory::GetForProfile(
      ProfileManager::GetActiveUserProfile());
  if (!app_service_proxy)
    return app_names;

  for (const auto& iter : launch_list) {
    std::string name;
    app_service_proxy->AppRegistryCache().ForOneApp(
        iter.first,
        [&name](const apps::AppUpdate& update) { name = update.ShortName(); });
    if (!IsAppAvailable(iter.first, app_service_proxy))
      app_names.push_back(name);
  }
  return app_names;
}

// Show unavailable app toast based on size of `unavailable_apps`.
void ShowUnavailableAppToast(const std::vector<std::string>& unavailable_apps) {
  std::u16string toast_string;
  switch (unavailable_apps.size()) {
    case 1:
      toast_string = l10n_util::GetStringFUTF16(
          IDS_ASH_DESKS_TEMPLATES_UNAVAILABLE_APP_TOAST_ONE,
          base::ASCIIToUTF16(unavailable_apps.front()));
      break;
    case 2:
      toast_string = l10n_util::GetStringFUTF16(
          IDS_ASH_DESKS_TEMPLATES_UNAVAILABLE_APP_TOAST_TWO,
          base::ASCIIToUTF16(unavailable_apps.front()),
          base::ASCIIToUTF16(unavailable_apps[1]));
      break;
    default:
      DCHECK_GT(unavailable_apps.size(), 2);
      toast_string = l10n_util::GetStringFUTF16(
          IDS_ASH_DESKS_TEMPLATES_UNAVAILABLE_APP_TOAST_MORE,
          base::ASCIIToUTF16(unavailable_apps.front()),
          base::ASCIIToUTF16(unavailable_apps[1]),
          base::FormatNumber((unavailable_apps.size() - 2)));
      break;
  }

  ash::ToastData toast_data = {/*id=*/kAppNotAvailableTemplateToastName,
                               /*text=*/
                               toast_string, kLaunchTemplateToastDurationMs,
                               /*dismiss_text=*/absl::nullopt};
  ash::ToastManager::Get()->Show(toast_data);
}

}  // namespace

ChromeDesksTemplatesDelegate::ChromeDesksTemplatesDelegate() = default;

ChromeDesksTemplatesDelegate::~ChromeDesksTemplatesDelegate() = default;

std::unique_ptr<app_restore::AppLaunchInfo>
ChromeDesksTemplatesDelegate::GetAppLaunchDataForDeskTemplate(
    aura::Window* window) const {
  const user_manager::User* active_user =
      user_manager::UserManager::Get()->GetActiveUser();
  DCHECK(active_user);
  Profile* user_profile =
      ash::ProfileHelper::Get()->GetProfileByUser(active_user);
  if (!user_profile)
    return nullptr;

  if (!IsWindowSupportedForDeskTemplate(window))
    return nullptr;

  // Get `full_restore_data` from FullRestoreSaveHandler which contains all
  // restoring information for all apps running on the device.
  const app_restore::RestoreData* full_restore_data =
      full_restore::FullRestoreSaveHandler::GetInstance()->GetRestoreData(
          user_profile->GetPath());
  DCHECK(full_restore_data);

  const std::string app_id = full_restore::GetAppId(window);
  DCHECK(!app_id.empty());

  const int32_t window_id = window->GetProperty(app_restore::kWindowIdKey);
  std::unique_ptr<app_restore::AppLaunchInfo> app_launch_info =
      std::make_unique<app_restore::AppLaunchInfo>(app_id, window_id);
  auto* tab_strip_model = GetTabstripModelForWindowIfAny(window);
  if (tab_strip_model) {
    app_launch_info->urls = GetURLsIfApplicable(tab_strip_model);
    app_launch_info->active_tab_index = tab_strip_model->active_index();
  }
  const std::string* app_name =
      window->GetProperty(app_restore::kBrowserAppNameKey);
  if (app_name)
    app_launch_info->app_name = *app_name;

  // Read all other relevant app launching information from `app_restore_data`
  // to `app_launch_info`.
  const app_restore::AppRestoreData* app_restore_data =
      full_restore_data->GetAppRestoreData(app_id, window_id);
  if (app_restore_data) {
    app_launch_info->app_type_browser = app_restore_data->app_type_browser;
    app_launch_info->event_flag = app_restore_data->event_flag;
    app_launch_info->container = app_restore_data->container;
    app_launch_info->disposition = app_restore_data->disposition;
    app_launch_info->file_paths = app_restore_data->file_paths;
    if (app_restore_data->intent.has_value() &&
        app_restore_data->intent.value()) {
      app_launch_info->intent = app_restore_data->intent.value()->Clone();
    }
  }

  auto& app_registry_cache =
      apps::AppServiceProxyFactory::GetForProfile(user_profile)
          ->AppRegistryCache();
  const apps::mojom::AppType app_type = app_registry_cache.GetAppType(app_id);
  if (app_id != extension_misc::kChromeAppId &&
      (app_type == apps::mojom::AppType::kChromeApp ||
       app_type == apps::mojom::AppType::kWeb)) {
    // If these values are not present, we will not be able to restore the
    // application. See http://crbug.com/1232520 for more information.
    if (!app_launch_info->container.has_value() ||
        !app_launch_info->disposition.has_value()) {
      return nullptr;
    }
  }

  return app_launch_info;
}

desks_storage::DeskModel* ChromeDesksTemplatesDelegate::GetDeskModel() {
  return DesksTemplatesClient::Get()->GetDeskModel();
}

bool ChromeDesksTemplatesDelegate::IsIncognitoWindow(
    aura::Window* window) const {
  BrowserView* browser_view =
      BrowserView::GetBrowserViewForNativeWindow(window);
  return browser_view && browser_view->GetIncognito();
}

absl::optional<gfx::ImageSkia>
ChromeDesksTemplatesDelegate::MaybeRetrieveIconForSpecialIdentifier(
    const std::string& identifier,
    const ui::ColorProvider* color_provider) const {
  if (identifier == chrome::kChromeUINewTabURL) {
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    return absl::make_optional<gfx::ImageSkia>(
        rb.GetImageNamed(IDR_PRODUCT_LOGO_32).AsImageSkia());
  } else if (identifier == ash::DeskTemplate::kIncognitoWindowIdentifier) {
    DCHECK(color_provider);
    return ui::ThemedVectorIcon(
               ui::ImageModel::FromVectorIcon(kIncognitoProfileIcon,
                                              ui::kColorAvatarIconIncognito)
                   .GetVectorIcon())
        .GetImageSkia(color_provider);
  }

  return absl::nullopt;
}

void ChromeDesksTemplatesDelegate::GetFaviconForUrl(
    const std::string& page_url,
    int desired_icon_size,
    favicon_base::FaviconRawBitmapCallback callback,
    base::CancelableTaskTracker* tracker) const {
  favicon::FaviconService* favicon_service =
      FaviconServiceFactory::GetForProfile(
          ProfileManager::GetActiveUserProfile(),
          ServiceAccessType::EXPLICIT_ACCESS);

  favicon_service->GetRawFaviconForPageURL(
      GURL(page_url), {favicon_base::IconType::kFavicon}, desired_icon_size,
      /*fallback_to_host=*/false, std::move(callback), tracker);
}

void ChromeDesksTemplatesDelegate::GetIconForAppId(
    const std::string& app_id,
    int desired_icon_size,
    base::OnceCallback<void(apps::IconValuePtr icon_value)> callback) const {
  auto* app_service_proxy = apps::AppServiceProxyFactory::GetForProfile(
      ProfileManager::GetActiveUserProfile());
  if (!app_service_proxy) {
    std::move(callback).Run(std::make_unique<apps::IconValue>());
    return;
  }

  auto app_type = app_service_proxy->AppRegistryCache().GetAppType(app_id);
  if (base::FeatureList::IsEnabled(features::kAppServiceLoadIconWithoutMojom)) {
    app_service_proxy->LoadIcon(
        apps::ConvertMojomAppTypToAppType(app_type), app_id,
        apps::IconType::kStandard, desired_icon_size,
        /*allow_placeholder_icon=*/false, std::move(callback));
  } else {
    app_service_proxy->LoadIcon(
        app_type, app_id, apps::mojom::IconType::kStandard, desired_icon_size,
        /*allow_placeholder_icon=*/false,
        apps::MojomIconValueToIconValueCallback(std::move(callback)));
  }
}

void ChromeDesksTemplatesDelegate::LaunchAppsFromTemplate(
    std::unique_ptr<ash::DeskTemplate> desk_template) {
  const auto& launch_list =
      desk_template->desk_restore_data()->app_id_to_launch_list();
  std::vector<std::string> unavailable_apps =
      GetUnavailableAppNames(launch_list);
  // Show app unavailable toast.
  if (!unavailable_apps.empty())
    ShowUnavailableAppToast(unavailable_apps);
  DesksTemplatesClient::Get()->LaunchAppsFromTemplate(std::move(desk_template));
}

// Returns true if `window` is supported in desk templates feature.
bool ChromeDesksTemplatesDelegate::IsWindowSupportedForDeskTemplate(
    aura::Window* window) const {
  if (!ash::DeskTemplate::IsAppTypeSupported(window))
    return false;

  // Exclude incognito browser window.
  return !IsIncognitoWindow(window);
}
