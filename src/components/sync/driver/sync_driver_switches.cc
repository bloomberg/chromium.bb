// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/sync_driver_switches.h"

#include "base/command_line.h"
#include "build/build_config.h"

namespace switches {

bool IsSyncAllowedByFlag() {
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableSync);
}

// Disables syncing browser data to a Google Account.
const char kDisableSync[] = "disable-sync";

// Allows overriding the deferred init fallback timeout.
const char kSyncDeferredStartupTimeoutSeconds[] =
    "sync-deferred-startup-timeout-seconds";

// Enables deferring sync backend initialization until user initiated changes
// occur.
const char kSyncDisableDeferredStartup[] = "sync-disable-deferred-startup";

// Controls whether the initial state of the "Capture Specifics" flag on
// chrome://sync-internals is enabled.
const char kSyncIncludeSpecificsInProtocolLog[] = "sync-include-specifics";

// This flag causes sync to retry very quickly (see polling_constants.h) the
// when it encounters an error, as the first step towards exponential backoff.
const char kSyncShortInitialRetryOverride[] =
    "sync-short-initial-retry-override";

// This flag significantly shortens the delay between nudge cycles. Its primary
// purpose is to speed up integration tests. The normal delay allows coalescing
// and prevention of server overload, so don't use this unless you're really
// sure that it's what you want.
const char kSyncShortNudgeDelayForTest[] = "sync-short-nudge-delay-for-test";

// Allows custom passphrase users to receive Wallet data for secondary accounts
// while in transport-only mode.
const base::Feature kSyncAllowWalletDataInTransportModeWithCustomPassphrase{
    "SyncAllowAutofillWalletDataInTransportModeWithCustomPassphrase",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether to enable syncing of Autofill Wallet offer data.
const base::Feature kSyncAutofillWalletOfferData{
    "SyncAutofillWalletOfferData", base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether to enable syncing of Wi-Fi configurations.
const base::Feature kSyncWifiConfigurations{"SyncWifiConfigurations",
                                            base::FEATURE_ENABLED_BY_DEFAULT};

// Stops honoring the Android master sync toggle.
const base::Feature kDecoupleSyncFromAndroidMasterSync{
    "DecoupleSyncFromAndroidMasterSync", base::FEATURE_DISABLED_BY_DEFAULT};

// Allows trusted vault implementation to follow key rotation (including device
// registration).
const base::Feature kFollowTrustedVaultKeyRotation{
    "FollowTrustedVaultKeyRotation", base::FEATURE_DISABLED_BY_DEFAULT};

// Allows device registration within trusted vault server without having trusted
// vault key. Effectively disabled if kFollowTrustedVaultKeyRotation is
// disabled.
const base::Feature kAllowSilentTrustedVaultDeviceRegistration{
    "AllowSilentTrustedVaultDeviceRegistration",
    base::FEATURE_ENABLED_BY_DEFAULT};

// Specifies how long requests to vault service shouldn't be retried after
// encountering transient error.
const base::FeatureParam<base::TimeDelta>
    kTrustedVaultServiceThrottlingDuration{
        &kFollowTrustedVaultKeyRotation,
        "TrustedVaultServiceThrottlingDuration", base::TimeDelta::FromDays(1)};

// Sync requires policies to be loaded before starting.
const base::Feature kSyncRequiresPoliciesLoaded{
    "SyncRequiresPoliciesLoaded", base::FEATURE_DISABLED_BY_DEFAULT};

// Max time to delay the sync startup while waiting for policies to load.
const base::FeatureParam<base::TimeDelta> kSyncPolicyLoadTimeout{
    &kSyncRequiresPoliciesLoaded, "SyncPolicyLoadTimeout",
    base::TimeDelta::FromSeconds(10)};

const base::Feature kSyncSupportTrustedVaultPassphraseRecovery{
    "SyncSupportTrustedVaultPassphraseRecovery",
    base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace switches
