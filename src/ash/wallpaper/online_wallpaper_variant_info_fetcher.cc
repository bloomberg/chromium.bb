// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/online_wallpaper_variant_info_fetcher.h"

#include <algorithm>

#include "ash/public/cpp/wallpaper/online_wallpaper_params.h"
#include "ash/public/cpp/wallpaper/online_wallpaper_variant.h"
#include "ash/public/cpp/wallpaper/wallpaper_controller_client.h"
#include "ash/public/cpp/wallpaper/wallpaper_info.h"
#include "ash/webui/personalization_app/proto/backdrop_wallpaper.pb.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/strings/string_piece.h"
#include "base/threading/sequenced_task_runner_handle.h"

namespace ash {
namespace {

// Convenience alias for ColorMode enum.
using ColorMode = OnlineWallpaperVariantInfoFetcher::ColorMode;

// Checks if the given |variant| is suitable for the current system's color
// mode. Image with type |Image_ImageType_IMAGE_TYPE_UNKNOWN| is not D/L aware
// and should be used regardless of color mode.
bool IsSuitableOnlineWallpaperVariant(const OnlineWallpaperVariant& variant,
                                      ColorMode mode) {
  switch (variant.type) {
    case backdrop::Image_ImageType_IMAGE_TYPE_UNKNOWN:
      return true;
    case backdrop::Image_ImageType_IMAGE_TYPE_LIGHT_MODE:
      return mode == ColorMode::kLightMode;
    case backdrop::Image_ImageType_IMAGE_TYPE_DARK_MODE:
      return mode == ColorMode::kDarkMode;
  }
}

// Returns a pointer to the first matching variant in |variants| if one
// exists.
const OnlineWallpaperVariant* FirstValidVariant(
    const std::vector<OnlineWallpaperVariant>& variants,
    ColorMode mode) {
  const auto iter = std::find_if(
      variants.begin(), variants.end(), [mode](const auto& variant) {
        return IsSuitableOnlineWallpaperVariant(variant, mode);
      });
  if (iter != variants.end())
    return &(*iter);

  return nullptr;
}

// The filtered results from a set of backdrop::Images for a given |asset_id|
// and |mode| value.
class VariantMatches {
 public:
  VariantMatches(VariantMatches&&) = default;

  VariantMatches(const VariantMatches&) = delete;
  VariantMatches& operator=(const VariantMatches&) = delete;

  ~VariantMatches() = default;

  // Filters |images| to only the entries that match |asset_id| and
  // |mode|.
  static absl::optional<VariantMatches> FromImages(
      uint64_t asset_id,
      ColorMode mode,
      const std::vector<backdrop::Image>& images) {
    // Find the exact image in the |images| collection.
    auto image_iter = std::find_if(images.begin(), images.end(),
                                   [asset_id](const backdrop::Image& image) {
                                     return asset_id == image.asset_id();
                                   });

    if (image_iter == images.end())
      return absl::nullopt;

    uint64_t unit_id = image_iter->unit_id();
    std::vector<OnlineWallpaperVariant> variants;
    for (const auto& image : images) {
      if (image.unit_id() == unit_id) {
        variants.emplace_back(image.asset_id(), GURL(image.image_url()),
                              image.has_image_type()
                                  ? image.image_type()
                                  : backdrop::Image::IMAGE_TYPE_UNKNOWN);
      }
    }

    const OnlineWallpaperVariant* variant = FirstValidVariant(variants, mode);
    if (!variant) {
      // At least one usable variant must be found to use this set of images.
      return absl::nullopt;
    }

    return VariantMatches(unit_id, std::move(variants), *variant);
  }

  // The unit id of the Variant that matched |asset_id| and |mode|.
  const uint64_t unit_id;

  // The set of images that are appropriate for |asset_id| and
  // |mode|.
  const std::vector<OnlineWallpaperVariant> variants;

  // The first instance from |variants| that matches |asset_id| and
  // |mode|.
  const OnlineWallpaperVariant first_match;

 private:
  VariantMatches(uint64_t unit_id_in,
                 std::vector<OnlineWallpaperVariant>&& variants_in,
                 const OnlineWallpaperVariant& first_match_in)
      : unit_id(unit_id_in),
        variants(variants_in),
        first_match(first_match_in) {
    DCHECK_EQ(std::count(variants.begin(), variants.end(), first_match), 1);
  }
};

bool IsDaily(const WallpaperInfo& info) {
  return info.type == WallpaperType::kDaily;
}

}  // namespace

OnlineWallpaperVariantInfoFetcher::OnlineWallpaperRequest::
    OnlineWallpaperRequest(const AccountId& account_id_in,
                           const std::string& collection_id_in,
                           WallpaperLayout layout_in,
                           bool daily_refresh_enabled_in,
                           ColorMode mode_in)
    : account_id(account_id_in),
      collection_id(collection_id_in),
      layout(layout_in),
      daily_refresh_enabled(daily_refresh_enabled_in),
      mode(mode_in) {}

OnlineWallpaperVariantInfoFetcher::OnlineWallpaperRequest::
    ~OnlineWallpaperRequest() = default;

OnlineWallpaperVariantInfoFetcher::OnlineWallpaperVariantInfoFetcher() =
    default;
OnlineWallpaperVariantInfoFetcher::~OnlineWallpaperVariantInfoFetcher() =
    default;

void OnlineWallpaperVariantInfoFetcher::SetClient(
    WallpaperControllerClient* client) {
  wallpaper_controller_client_ = client;
}

void OnlineWallpaperVariantInfoFetcher::FetchOnlineWallpaper(
    const AccountId& account_id,
    const WallpaperInfo& info,
    ColorMode mode,
    FetchParamsCallback callback) {
  DCHECK(info.type == WallpaperType::kDaily ||
         info.type == WallpaperType::kOnline);

  DCHECK(wallpaper_controller_client_);

  if (info.unit_id.has_value() && !info.variants.empty()) {
    // |info| already has all of the data we need.
    const OnlineWallpaperVariant* variant =
        FirstValidVariant(info.variants, mode);
    if (!variant) {
      NOTREACHED() << "No suitable wallpaper for "
                   << (mode == ColorMode::kDarkMode ? "dark" : "lite")
                   << " mode in collection";
      base::SequencedTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), absl::nullopt));
      return;
    }

    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(callback),
            OnlineWallpaperParams{account_id, variant->asset_id,
                                  GURL(variant->raw_url), info.collection_id,
                                  info.layout, /*preview_mode=*/false,
                                  /*from_user=*/false, IsDaily(info),
                                  info.unit_id, info.variants}));
    return;
  }

  // For requests from existing WallpaperInfo, asset_id is always populated.
  DCHECK(info.asset_id.has_value());

  bool daily = IsDaily(info);
  auto request = std::make_unique<OnlineWallpaperRequest>(
      account_id, info.collection_id, info.layout, daily, mode);

  auto collection_id = request->collection_id;
  wallpaper_controller_client_->FetchImagesForCollection(
      collection_id,
      base::BindOnce(
          &OnlineWallpaperVariantInfoFetcher::FindAndSetOnlineWallpaperVariants,
          weak_factory_.GetWeakPtr(), std::move(request), *info.asset_id,
          std::move(callback)));
}

bool OnlineWallpaperVariantInfoFetcher::FetchDailyWallpaper(
    const AccountId& account_id,
    const WallpaperInfo& info,
    ColorMode mode,
    FetchParamsCallback callback) {
  DCHECK(IsDaily(info));

  // We might not have a client yet.
  if (!wallpaper_controller_client_)
    return false;

  bool daily = true;  // This is always a a daily wallpaper.
  auto request = std::make_unique<OnlineWallpaperRequest>(
      account_id, info.collection_id, info.layout, daily, mode);
  wallpaper_controller_client_->FetchDailyRefreshWallpaper(
      info.collection_id,
      base::BindOnce(&OnlineWallpaperVariantInfoFetcher::OnSingleFetch,
                     weak_factory_.GetWeakPtr(), std::move(request),
                     std::move(callback)));
  return true;
}

void OnlineWallpaperVariantInfoFetcher::OnSingleFetch(
    std::unique_ptr<OnlineWallpaperRequest> request,
    FetchParamsCallback callback,
    bool success,
    const backdrop::Image& image) {
  if (!success) {
    std::move(callback).Run(absl::nullopt);
    return;
  }

  // If wallpaper_controller_client_ is null, we're shutting down.
  if (!wallpaper_controller_client_)
    return;

  auto collection_id = request->collection_id;
  wallpaper_controller_client_->FetchImagesForCollection(
      collection_id,
      base::BindOnce(
          &OnlineWallpaperVariantInfoFetcher::FindAndSetOnlineWallpaperVariants,
          weak_factory_.GetWeakPtr(), std::move(request), image.asset_id(),
          std::move(callback)));
}

void OnlineWallpaperVariantInfoFetcher::FindAndSetOnlineWallpaperVariants(
    std::unique_ptr<OnlineWallpaperRequest> request,
    uint64_t asset_id,
    FetchParamsCallback callback,
    bool success,
    const std::vector<backdrop::Image>& images) {
  if (!success) {
    LOG(WARNING) << "Failed to fetch online wallpapers";
    std::move(callback).Run(absl::nullopt);
    return;
  }

  absl::optional<VariantMatches> matches =
      VariantMatches::FromImages(asset_id, request->mode, images);
  if (!matches) {
    LOG(ERROR) << "No valid variants";
    std::move(callback).Run(absl::nullopt);
    return;
  }

  const OnlineWallpaperVariant& first_image = matches->first_match;
  DCHECK(IsSuitableOnlineWallpaperVariant(first_image, request->mode));

  std::move(callback).Run(ash::OnlineWallpaperParams{
      request->account_id, first_image.asset_id, first_image.raw_url,
      request->collection_id, request->layout, /*preview_mode=*/false,
      /*from_user=*/false, request->daily_refresh_enabled, matches->unit_id,
      matches->variants});
}

}  // namespace ash
