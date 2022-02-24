// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_WALLPAPER_GOOGLE_PHOTOS_WALLPAPER_PARAMS_H_
#define ASH_PUBLIC_CPP_WALLPAPER_GOOGLE_PHOTOS_WALLPAPER_PARAMS_H_

#include <string>

#include "ash/public/cpp/ash_public_export.h"
#include "components/account_id/account_id.h"

namespace ash {

struct ASH_PUBLIC_EXPORT GooglePhotosWallpaperParams {
  GooglePhotosWallpaperParams(const AccountId& account_id,
                              const std::string& id);

  GooglePhotosWallpaperParams(const GooglePhotosWallpaperParams& other);

  GooglePhotosWallpaperParams& operator=(
      const GooglePhotosWallpaperParams& other);

  ~GooglePhotosWallpaperParams();

  // The user's account id.
  AccountId account_id;

  // The unique identifier for the photo.
  std::string id;
};

ASH_PUBLIC_EXPORT std::ostream& operator<<(
    std::ostream& os,
    const GooglePhotosWallpaperParams& params);

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_WALLPAPER_GOOGLE_PHOTOS_WALLPAPER_PARAMS_H_
