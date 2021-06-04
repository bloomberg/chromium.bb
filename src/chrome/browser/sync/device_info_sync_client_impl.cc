// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/device_info_sync_client_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sharing/sharing_sync_preference.h"
#include "chrome/browser/signin/chrome_device_id_helper.h"
#include "chrome/browser/sync/sync_invalidations_service_factory.h"
#include "components/send_tab_to_self/features.h"
#include "components/sync/base/sync_prefs.h"
#include "components/sync/invalidations/sync_invalidations_service.h"

#if defined(OS_ANDROID)
#include "chrome/browser/webauthn/android/cable_module_android.h"
#endif

namespace browser_sync {

DeviceInfoSyncClientImpl::DeviceInfoSyncClientImpl(Profile* profile)
    : profile_(profile) {}

DeviceInfoSyncClientImpl::~DeviceInfoSyncClientImpl() = default;

// syncer::DeviceInfoSyncClient:
std::string DeviceInfoSyncClientImpl::GetSigninScopedDeviceId() const {
// Since the local sync backend is currently only supported on Windows, Mac and
// Linux don't even check the pref on other os-es.
// TODO(crbug.com/1052397): Reassess whether the next block needs to be included
// in lacros-chrome once build flag switch of lacros-chrome is
// complete.
#if defined(OS_WIN) || defined(OS_MAC) || \
    (defined(OS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))
  syncer::SyncPrefs prefs(profile_->GetPrefs());
  if (prefs.IsLocalSyncEnabled()) {
    return "local_device";
  }
#endif  // defined(OS_WIN) || defined(OS_MAC) || (defined(OS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS_LACROS))

  return GetSigninScopedDeviceIdForProfile(profile_);
}

// syncer::DeviceInfoSyncClient:
bool DeviceInfoSyncClientImpl::GetSendTabToSelfReceivingEnabled() const {
  return send_tab_to_self::IsReceivingEnabledByUserOnThisDevice(
      profile_->GetPrefs());
}

// syncer::DeviceInfoSyncClient:
absl::optional<syncer::DeviceInfo::SharingInfo>
DeviceInfoSyncClientImpl::GetLocalSharingInfo() const {
  return SharingSyncPreference::GetLocalSharingInfoForSync(
      profile_->GetPrefs());
}

// syncer::DeviceInfoSyncClient:
absl::optional<std::string> DeviceInfoSyncClientImpl::GetFCMRegistrationToken()
    const {
  syncer::SyncInvalidationsService* service =
      SyncInvalidationsServiceFactory::GetForProfile(profile_);
  if (service) {
    return service->GetFCMRegistrationToken();
  }
  // If the service is not enabled, then the registration token must be empty,
  // not unknown (absl::nullopt). This is needed to reset previous token if
  // the invalidations have been turned off.
  return std::string();
}

// syncer::DeviceInfoSyncClient:
absl::optional<syncer::ModelTypeSet>
DeviceInfoSyncClientImpl::GetInterestedDataTypes() const {
  syncer::SyncInvalidationsService* service =
      SyncInvalidationsServiceFactory::GetForProfile(profile_);
  if (service) {
    return service->GetInterestedDataTypes();
  }
  // If the service is not enabled, then the list of types must be empty, not
  // unknown (absl::nullopt). This is needed to reset previous types if the
  // invalidations have been turned off.
  return syncer::ModelTypeSet();
}

absl::optional<syncer::DeviceInfo::PhoneAsASecurityKeyInfo>
DeviceInfoSyncClientImpl::GetPhoneAsASecurityKeyInfo() const {
#if defined(OS_ANDROID)
  return webauthn::authenticator::GetSyncDataIfRegistered();
#else
  return absl::nullopt;
#endif
}

bool DeviceInfoSyncClientImpl::IsUmaEnabledOnCrOSDevice() const {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled();
#else
  return false;
#endif
}
}  // namespace browser_sync
