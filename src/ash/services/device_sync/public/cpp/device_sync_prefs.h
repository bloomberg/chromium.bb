// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SERVICES_DEVICE_SYNC_PUBLIC_CPP_DEVICE_SYNC_PREFS_H_
#define ASH_SERVICES_DEVICE_SYNC_PUBLIC_CPP_DEVICE_SYNC_PREFS_H_

class PrefRegistrySimple;

namespace ash {
namespace device_sync {

// Register's Device Sync's profile preferences for browser prefs.
void RegisterProfilePrefs(PrefRegistrySimple* registry);

}  // namespace device_sync
}  // namespace ash

// TODO(https://crbug.com/1164001): remove when the migration is finished.
namespace chromeos::device_sync {
using ::ash::device_sync::RegisterProfilePrefs;
}

#endif  // ASH_SERVICES_DEVICE_SYNC_PUBLIC_CPP_DEVICE_SYNC_PREFS_H_
