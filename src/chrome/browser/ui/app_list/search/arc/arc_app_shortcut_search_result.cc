// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/arc/arc_app_shortcut_search_result.h"

#include <string>
#include <utility>

#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_factory.h"
#include "chrome/browser/chromeos/arc/icon_decode_request.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/app_service/app_service_app_icon_loader.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/app_list/search/search_tags_util.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"

namespace app_list {

namespace {
constexpr char kAppShortcutSearchPrefix[] = "appshortcutsearch://";
}  // namespace

ArcAppShortcutSearchResult::ArcAppShortcutSearchResult(
    arc::mojom::AppShortcutItemPtr data,
    Profile* profile,
    AppListControllerDelegate* list_controller,
    bool is_recommendation,
    const std::u16string& query)
    : data_(std::move(data)),
      profile_(profile),
      list_controller_(list_controller) {
  const auto title = base::UTF8ToUTF16(data_->short_label);
  SetTitle(title);
  if (!query.empty())
    SetTitleTags(CalculateTags(query, title));
  set_id(kAppShortcutSearchPrefix + GetAppId() + "/" + data_->shortcut_id);
  SetCategory(Category::kAppShortcuts);
  SetAccessibleName(ComputeAccessibleName());
  SetResultType(ash::AppListSearchResultType::kArcAppShortcut);
  SetDisplayType(ash::SearchResultDisplayType::kTile);
  SetMetricsType(ash::PLAY_STORE_APP_SHORTCUT);
  SetIsRecommendation(is_recommendation);

  if (!data_->icon || !data_->icon->icon_png_data ||
      data_->icon->icon_png_data->empty()) {
    UMA_HISTOGRAM_ENUMERATION("Arc.AppShortcutSearchResult.ShortcutStatus",
                              arc::ArcAppShortcutStatus::kEmpty);
  } else {
    UMA_HISTOGRAM_ENUMERATION("Arc.AppShortcutSearchResult.ShortcutStatus",
                              arc::ArcAppShortcutStatus::kNotEmpty);
  }

  const int icon_dimension =
      ash::SharedAppListConfig::instance().search_tile_icon_dimension();
  DCHECK(data_->icon);
  apps::ArcRawIconPngDataToImageSkia(
      std::move(data_->icon), icon_dimension,
      base::BindOnce(&ArcAppShortcutSearchResult::OnIconDecoded,
                     weak_ptr_factory_.GetWeakPtr()));

  badge_icon_loader_ = std::make_unique<AppServiceAppIconLoader>(
      profile_,
      ash::SharedAppListConfig::instance().search_tile_badge_icon_dimension(),
      this);
  badge_icon_loader_->FetchImage(GetAppId());
}

ArcAppShortcutSearchResult::~ArcAppShortcutSearchResult() = default;

void ArcAppShortcutSearchResult::Open(int event_flags) {
  arc::LaunchAppShortcutItem(profile_, GetAppId(), data_->shortcut_id,
                             list_controller_->GetAppListDisplayId());
}

void ArcAppShortcutSearchResult::OnAppImageUpdated(
    const std::string& app_id,
    const gfx::ImageSkia& image) {
  SetBadgeIcon(ui::ImageModel::FromImageSkia(image));
}

std::string ArcAppShortcutSearchResult::GetAppId() const {
  if (!data_->package_name)
    return std::string();
  const ArcAppListPrefs* arc_prefs = ArcAppListPrefs::Get(profile_);
  DCHECK(arc_prefs);
  return arc_prefs->GetAppIdByPackageName(data_->package_name.value());
}

std::u16string ArcAppShortcutSearchResult::ComputeAccessibleName() const {
  const ArcAppListPrefs* arc_prefs = ArcAppListPrefs::Get(profile_);
  DCHECK(arc_prefs);
  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
      arc_prefs->GetApp(GetAppId());
  if (!app_info.get())
    return std::u16string();

  return l10n_util::GetStringFUTF16(IDS_APP_ACTION_SHORTCUT_ACCESSIBILITY_NAME,
                                    base::UTF8ToUTF16(data_->short_label),
                                    base::UTF8ToUTF16(app_info->name));
}

void ArcAppShortcutSearchResult::OnIconDecoded(const gfx::ImageSkia& icon) {
  SetIcon(IconInfo(icon));
}

}  // namespace app_list
