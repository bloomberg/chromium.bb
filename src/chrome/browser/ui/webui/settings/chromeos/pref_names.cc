// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/pref_names.h"

namespace chromeos {
namespace settings {
namespace prefs {

// Boolean specifying whether OS wallpaper sync is enabled. This is stored
// separately from the other OS sync preferences because it's an edge case;
// wallpaper does not have its own ModelType, so it cannot be part of
// UserSelectableOsType like the other OS sync types.
//
// Please note that this is only relevant if IsSyncAllOsTypesEnabled() is false,
// so callers have to check both this pref and IsSyncAllOsTypesEnabled() to
// verify whether this wallpaper sync is enabled.
// TODO(https://crbug.com/1318106): Create a helper method that checks both.
const char kSyncOsWallpaper[] = "sync.os_wallpaper";

}  // namespace prefs
}  // namespace settings
}  // namespace chromeos
