// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_FEATURES_H_
#define CHROME_BROWSER_SHARING_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

// Feature flag to enable QR Code Generator (currently desktop-only).
extern const base::Feature kSharingQRCodeGenerator;

// Feature flag for configuring device expiration.
extern const base::Feature kSharingDeviceExpiration;

// The number of hours after which a device is considered expired.
extern const base::FeatureParam<int> kSharingDeviceExpirationHours;

// Feature flag for matching device expiration to pulse interval.
extern const base::Feature kSharingMatchPulseInterval;

// The delta from the pulse interval in hours after which a device is considered
// expired, for Desktop devices. Chrome on Desktop is expected to update the
// last updated timestamp quite frequently because it can do this when
// backgrounded. Such devices can be marked stale aggressively if they did not
// update for more than an interval.
extern const base::FeatureParam<int> kSharingPulseDeltaDesktopHours;

// The delta from the pulse interval in hours after which a device is considered
// expired, for Android devices. Chrome on Android is expected to update the
// last updated timestamp less frequently because it does not do this when
// backgrounded. Such devices cannot be marked stale aggressively.
extern const base::FeatureParam<int> kSharingPulseDeltaAndroidHours;

// Feature flag for configuring the sharing message timeout. This sets both the
// FCM message TTL and the duration of the timer that waits for an ack.
extern const base::Feature kSharingMessageTTL;

// The duration in seconds for both the FCM message TTL and the timer that waits
// for an ack.
extern const base::FeatureParam<int> kSharingMessageTTLSeconds;

// Feature flag for configuring the FCM TTL for sharing ack messages.
extern const base::Feature kSharingAckMessageTTL;

// The FCM TTL in seconds for sharing ack messages.
extern const base::FeatureParam<int> kSharingAckMessageTTLSeconds;

// Feature flag for configuring the timeout in the sharing message bridge.
extern const base::Feature kSharingMessageBridgeTimeout;
extern const base::FeatureParam<int> kSharingMessageBridgeTimeoutSeconds;

// Feature flag for sending sharing message via Sync.
extern const base::Feature kSharingSendViaSync;

// Feature flag for prefer sending sharing message using VAPID.
extern const base::Feature kSharingPreferVapid;

#endif  // CHROME_BROWSER_SHARING_FEATURES_H_
