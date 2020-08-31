// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_SHARING_UTILS_H_
#define CHROME_BROWSER_SHARING_SHARING_UTILS_H_

#include <string>

#include "base/optional.h"

namespace chrome_browser_sharing {
class FCMChannelConfiguration;
}  // namespace chrome_browser_sharing

namespace syncer {
class DeviceInfo;
class SyncService;
}  // namespace syncer

enum class SharingDevicePlatform;

// Returns true if can send messages via VAPID.
bool CanSendViaVapid(syncer::SyncService* sync_service);

// Returns true if can send messages via sedner ID.
bool CanSendViaSenderID(syncer::SyncService* sync_service);

// Returns true if required sync feature is enabled.
bool IsSyncEnabledForSharing(syncer::SyncService* sync_service);

// Returns true if required sync feature is disabled.
bool IsSyncDisabledForSharing(syncer::SyncService* sync_service);

// Returns the FCMChannelConfiguration of device with specified |device_info|.
base::Optional<chrome_browser_sharing::FCMChannelConfiguration> GetFCMChannel(
    const syncer::DeviceInfo& device_info);

// Returns the SharingDevicePlatform of device with specified |device_info|.
SharingDevicePlatform GetDevicePlatform(const syncer::DeviceInfo& device_info);

#endif  // CHROME_BROWSER_SHARING_SHARING_UTILS_H_
