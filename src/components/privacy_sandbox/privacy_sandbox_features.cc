// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/privacy_sandbox/privacy_sandbox_features.h"

namespace privacy_sandbox {

const base::Feature kPrivacySandboxSettings3{"PrivacySandboxSettings3",
                                             base::FEATURE_DISABLED_BY_DEFAULT};
const base::FeatureParam<bool> kPrivacySandboxSettings3ConsentRequired{
    &kPrivacySandboxSettings3, "consent-required", false};
const base::FeatureParam<bool> kPrivacySandboxSettings3DisableDialogForTesting{
    &kPrivacySandboxSettings3, "disable-dialog-for-testing", false};
const base::FeatureParam<bool>
    kPrivacySandboxSettings3ForceShowConsentForTesting{
        &kPrivacySandboxSettings3, "force-show-consent-for-testing", false};
const base::FeatureParam<bool>
    kPrivacySandboxSettings3ForceShowNoticeForTesting{
        &kPrivacySandboxSettings3, "force-show-notice-for-testing", false};

}  // namespace privacy_sandbox
