// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/arc/arc_playstore_search_result.h"

#include <utility>

#include "ash/components/arc/mojom/app.mojom.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/vector_icons/vector_icons.h"
#include "base/bind.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_factory.h"
#include "chrome/browser/chromeos/arc/icon_decode_request.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/arc/arc_playstore_app_context_menu.h"
#include "chrome/browser/ui/app_list/search/search_tags_util.h"
#include "components/crx_file/id_util.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"

namespace {

// The id prefix to identify a Play Store search result.
constexpr char kPlayAppPrefix[] = "play://";
// Badge icon color.
constexpr SkColor kBadgeColor = gfx::kGoogleGrey700;
// Size of the vector icon inside the badge.
constexpr int kBadgeIconSize = 12;

// The background image source for badge.
class BadgeBackgroundImageSource : public gfx::CanvasImageSource {
 public:
  explicit BadgeBackgroundImageSource(int size)
      : CanvasImageSource(gfx::Size(size, size)) {}

  BadgeBackgroundImageSource(const BadgeBackgroundImageSource&) = delete;
  BadgeBackgroundImageSource& operator=(const BadgeBackgroundImageSource&) =
      delete;

  ~BadgeBackgroundImageSource() override = default;

 private:
  // gfx::CanvasImageSource overrides:
  void Draw(gfx::Canvas* canvas) override {
    cc::PaintFlags flags;
    flags.setColor(SK_ColorWHITE);
    flags.setAntiAlias(true);
    flags.setStyle(cc::PaintFlags::kFill_Style);
    const float origin = static_cast<float>(size().width()) / 2;
    canvas->DrawCircle(gfx::PointF(origin, origin), origin, flags);
  }
};

gfx::ImageSkia CreateBadgeIcon(const gfx::VectorIcon& vector_icon,
                               int badge_size,
                               int icon_size,
                               SkColor icon_color) {
  gfx::ImageSkia background(
      std::make_unique<BadgeBackgroundImageSource>(badge_size),
      gfx::Size(badge_size, badge_size));

  gfx::ImageSkia foreground(
      gfx::CreateVectorIcon(vector_icon, icon_size, icon_color));

  return gfx::ImageSkiaOperations::CreateSuperimposedImage(background,
                                                           foreground);
}

bool LaunchIntent(const std::string& intent_uri, int64_t display_id) {
  auto* arc_service_manager = arc::ArcServiceManager::Get();
  if (!arc_service_manager)
    return false;

  auto* arc_bridge = arc_service_manager->arc_bridge_service();

  if (auto* app_instance =
          ARC_GET_INSTANCE_FOR_METHOD(arc_bridge->app(), LaunchIntent)) {
    app_instance->LaunchIntent(intent_uri, display_id);
    return true;
  }

  if (auto* app_instance = ARC_GET_INSTANCE_FOR_METHOD(
          arc_bridge->app(), LaunchIntentDeprecated)) {
    app_instance->LaunchIntentDeprecated(intent_uri, absl::nullopt);
    return true;
  }

  return false;
}

}  // namespace

namespace app_list {

ArcPlayStoreSearchResult::ArcPlayStoreSearchResult(
    arc::mojom::AppDiscoveryResultPtr data,
    Profile* profile,
    AppListControllerDelegate* list_controller,
    const std::u16string& query)
    : data_(std::move(data)),
      profile_(profile),
      list_controller_(list_controller) {
  const auto title = base::UTF8ToUTF16(label().value());
  SetTitle(title);
  SetTitleTags(CalculateTags(query, title));
  set_id(kPlayAppPrefix +
         crx_file::id_util::GenerateId(install_intent_uri().value()));
  SetCategory(Category::kPlayStore);
  SetDisplayType(ash::SearchResultDisplayType::kTile);
  // TODO: The badge icon should be updated to pass through a vector icon and
  // color id rather than hardcoding the colors here.
  SetBadgeIcon(ui::ImageModel::FromImageSkia(CreateBadgeIcon(
      is_instant_app() ? ash::kBadgeInstantIcon : ash::kBadgePlayIcon,
      ash::SharedAppListConfig::instance().search_tile_badge_icon_dimension(),
      kBadgeIconSize, kBadgeColor)));
  SetFormattedPrice(base::UTF8ToUTF16(formatted_price().value()));
  SetRating(review_score());
  SetResultType(is_instant_app() ? ash::AppListSearchResultType::kInstantApp
                                 : ash::AppListSearchResultType::kPlayStoreApp);
  SetMetricsType(is_instant_app() ? ash::PLAY_STORE_INSTANT_APP
                                  : ash::PLAY_STORE_UNINSTALLED_APP);

  apps::ArcRawIconPngDataToImageSkia(
      std::move(data_->icon),
      ash::SharedAppListConfig::instance().search_tile_icon_dimension(),
      base::BindOnce(&ArcPlayStoreSearchResult::OnIconDecoded,
                     weak_ptr_factory_.GetWeakPtr()));
}

ArcPlayStoreSearchResult::~ArcPlayStoreSearchResult() = default;

void ArcPlayStoreSearchResult::Open(int event_flags) {
  LaunchIntent(install_intent_uri().value(),
               list_controller_->GetAppListDisplayId());
}

void ArcPlayStoreSearchResult::GetContextMenuModel(
    GetMenuModelCallback callback) {
  context_menu_ = std::make_unique<ArcPlayStoreAppContextMenu>(
      this, profile_, list_controller_);
  // TODO(755701): Enable context menu once Play Store API starts returning both
  // install and launch intents.
  std::move(callback).Run(nullptr);
}

void ArcPlayStoreSearchResult::ExecuteLaunchCommand(int event_flags) {
  Open(event_flags);
}

AppContextMenu* ArcPlayStoreSearchResult::GetAppContextMenu() {
  return context_menu_.get();
}

void ArcPlayStoreSearchResult::OnIconDecoded(const gfx::ImageSkia& icon) {
  SetIcon(IconInfo(icon));
}

}  // namespace app_list
