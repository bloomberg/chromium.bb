// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/personalization_provider.h"

#include <string>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_metrics.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/webui/personalization_app/personalization_app_url_constants.h"
#include "ash/webui/personalization_app/search/search.mojom.h"
#include "ash/webui/personalization_app/search/search_handler.h"
#include "base/bind.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/web_applications/personalization_app/personalization_app_manager.h"
#include "chrome/browser/ash/web_applications/personalization_app/personalization_app_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/search/common/icon_constants.h"
#include "chrome/browser/ui/web_applications/system_web_app_ui_utils.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/web_applications/web_app_id_constants.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_features.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "url/gurl.h"

namespace app_list {

namespace {

inline constexpr size_t kMinQueryLength = 3u;

}  // namespace

PersonalizationResult::PersonalizationResult(
    Profile* profile,
    const ash::personalization_app::mojom::SearchResult& result,
    const std::u16string& query,
    gfx::ImageSkia icon)
    : profile_(profile) {
  DCHECK(profile_);
  set_id(::ash::personalization_app::kChromeUIPersonalizationAppURL +
         result.relative_url);
  set_relevance(result.relevance_score);
  // Put personalization results into the Settings category. May change in the
  // future.
  SetCategory(Category::kSettings);
  SetTitle(result.text);
  SetResultType(ResultType::kPersonalization);
  SetDisplayType(DisplayType::kList);
  SetIcon(IconInfo(icon, GetAppIconDimension()));
  SetMetricsType(::ash::SearchResultType::PERSONALIZATION);
}

PersonalizationResult::~PersonalizationResult() = default;

void PersonalizationResult::Open(int event_flags) {
  ::web_app::SystemAppLaunchParams launch_params;
  launch_params.url = GURL(id());
  web_app::LaunchSystemWebAppAsync(
      profile_, web_app::SystemAppType::PERSONALIZATION, launch_params);
}

PersonalizationProvider::PersonalizationProvider(Profile* profile)
    : profile_(profile) {
  DCHECK(profile_);

  if (!ash::features::IsPersonalizationHubEnabled()) {
    return;
  }

  app_service_proxy_ = apps::AppServiceProxyFactory::GetForProfile(profile_);
  Observe(&app_service_proxy_->AppRegistryCache());
  StartLoadIcon();

  search_handler_ = ash::personalization_app::PersonalizationAppManagerFactory::
                        GetForBrowserContext(profile_)
                            ->search_handler();
}

PersonalizationProvider::~PersonalizationProvider() = default;

void PersonalizationProvider::Start(const std::u16string& query) {
  ClearResultsSilently();

  if (query.size() < kMinQueryLength) {
    return;
  }

  if (!search_handler_) {
    return;
  }

  if (icon_.isNull()) {
    return;
  }

  current_query_ = query;
  weak_ptr_factory_.InvalidateWeakPtrs();
  search_handler_->Search(query,
                          base::BindOnce(&PersonalizationProvider::OnSearchDone,
                                         weak_ptr_factory_.GetWeakPtr()));
}

ash::AppListSearchResultType PersonalizationProvider::ResultType() const {
  return ash::AppListSearchResultType::kPersonalization;
}

void PersonalizationProvider::ViewClosing() {
  current_query_.clear();
}

void PersonalizationProvider::OnAppUpdate(const apps::AppUpdate& update) {
  if (update.AppId() != web_app::kPersonalizationAppId) {
    return;
  }

  if (update.IconKeyChanged() ||
      (update.ReadinessChanged() &&
       update.Readiness() == apps::Readiness::kReady)) {
    StartLoadIcon();
  }
}

void PersonalizationProvider::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {}

void PersonalizationProvider::OnSearchDone(
    std::vector<::ash::personalization_app::mojom::SearchResultPtr> results) {
  SearchProvider::Results search_results;
  for (const auto& result : results) {
    DCHECK(!result.is_null());
    search_results.push_back(std::make_unique<PersonalizationResult>(
        profile_, *result, current_query_, icon_));
  }
  SwapResults(&search_results);
}

void PersonalizationProvider::StartLoadIcon() {
  apps::AppType app_type = app_service_proxy_->AppRegistryCache().GetAppType(
      web_app::kPersonalizationAppId);

  if (base::FeatureList::IsEnabled(features::kAppServiceLoadIconWithoutMojom)) {
    app_service_proxy_->LoadIcon(
        app_type, web_app::kPersonalizationAppId, apps::IconType::kStandard,
        ash::SharedAppListConfig::instance().search_list_icon_dimension(),
        /*allow_placeholder_icon=*/false,
        base::BindOnce(&PersonalizationProvider::OnLoadIcon,
                       app_service_weak_ptr_factory_.GetWeakPtr()));

  } else {
    app_service_proxy_->LoadIcon(
        apps::ConvertAppTypeToMojomAppType(app_type),
        web_app::kPersonalizationAppId, apps::mojom::IconType::kStandard,
        ash::SharedAppListConfig::instance().search_list_icon_dimension(),
        /*allow_placeholder_icon=*/false,
        apps::MojomIconValueToIconValueCallback(
            base::BindOnce(&PersonalizationProvider::OnLoadIcon,
                           app_service_weak_ptr_factory_.GetWeakPtr())));
  }
}

void PersonalizationProvider::OnLoadIcon(::apps::IconValuePtr icon_value) {
  if (icon_value && icon_value->icon_type == apps::IconType::kStandard) {
    icon_ = icon_value->uncompressed;
  }
}

}  // namespace app_list
