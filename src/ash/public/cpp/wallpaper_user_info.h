// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_WALLPAPER_USER_INFO_H_
#define ASH_PUBLIC_CPP_WALLPAPER_USER_INFO_H_

#include "ash/public/cpp/ash_public_export.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user_type.h"

namespace ash {

// User info needed to set wallpapers. Clients must specify the user because
// it's not always the same as the active user, e.g., when showing wallpapers
// for different user pods at login screen, or setting wallpapers selectively
// for primary user and active user during a multi-profile session.
struct ASH_PUBLIC_EXPORT WallpaperUserInfo {
  // The user's account id.
  AccountId account_id = EmptyAccountId();

  // The user type.
  user_manager::UserType type = user_manager::USER_TYPE_REGULAR;

  // True if the user's non-cryptohome data (wallpaper, avatar etc.) is
  // ephemeral. See |UserManager::IsCurrentUserNonCryptohomeDataEphemeral| for
  // more details.
  bool is_ephemeral = false;

  // True if the user has gaia account.
  bool has_gaia_account = false;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_WALLPAPER_USER_INFO_H_
