// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_FEATURES_H_
#define DEVICE_FIDO_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"

namespace url {
class Origin;
}

namespace device {

#if defined(OS_WIN)
// Controls whether on Windows, U2F/CTAP2 requests are forwarded to the
// native WebAuthentication API, where available.
COMPONENT_EXPORT(DEVICE_FIDO)
extern const base::Feature kWebAuthUseNativeWinApi;
#endif  // defined(OS_WIN)

// Enable biometric enrollment in the security keys settings UI.
COMPONENT_EXPORT(DEVICE_FIDO)
extern const base::Feature kWebAuthBiometricEnrollment;

// Enable using a phone as a generic security key.
COMPONENT_EXPORT(DEVICE_FIDO)
extern const base::Feature kWebAuthPhoneSupport;

// Enable WebAuthn GetAssertion calls in cross-origin iframes if allowed by
// Feature Policy.
COMPONENT_EXPORT(DEVICE_FIDO)
extern const base::Feature kWebAuthGetAssertionFeaturePolicy;

#if defined(OS_CHROMEOS) || defined(OS_LINUX)
// Use a low connection latency BLE mode when connecting to caBLE
// authenticators.
COMPONENT_EXPORT(DEVICE_FIDO)
extern const base::Feature kWebAuthCableLowLatency;
#endif  // defined(OS_CHROMEOS) || defined(OS_LINUX)

#if defined(OS_CHROMEOS)
// Enable a ChromeOS platform authenticator
COMPONENT_EXPORT(DEVICE_FIDO)
extern const base::Feature kWebAuthCrosPlatformAuthenticator;
#endif  // defined(OS_CHROMEOS)

COMPONENT_EXPORT(DEVICE_FIDO)
extern const base::Feature kWebAuthAttestationBlockList;
COMPONENT_EXPORT(DEVICE_FIDO)
extern const base::FeatureParam<std::string> kWebAuthAttestationBlockedDomains;

// DoesMatchWebAuthAttestationBlockedDomains returns true if the
// |kWebAuthAttestationBlocked| feature is enabled and |origin| is listed
// in |kWebAuthAttestationBlockedDomains|.
COMPONENT_EXPORT(DEVICE_FIDO)
bool DoesMatchWebAuthAttestationBlockedDomains(const url::Origin& origin);

}  // namespace device

#endif  // DEVICE_FIDO_FEATURES_H_
