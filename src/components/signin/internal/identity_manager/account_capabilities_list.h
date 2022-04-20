// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file intentionally does not have header guards, it's included
// inside a macro to generate a list of constants. The following line silences a
// presubmit and Tricium warning that would otherwise be triggered by this:
// no-include-guard-because-multiply-included
// NOLINT(build/header_guard)

// This is the list of account capabilities identifiers and their values. For
// the constant declarations, include the file
// "account_capabilities_constants.h".

// Here we define the values using a macro ACCOUNT_CAPABILITY, so it can be
// expanded differently in some places. The macro has the following signature:
// ACCOUNT_CAPABILITY(cpp_label, java_label, name).

ACCOUNT_CAPABILITY(kIsSubjectToParentalControlsCapabilityName,
                   IS_SUBJECT_TO_PARENTAL_CONTROLS_CAPABILITY_NAME,
                   "accountcapabilities/guydolldmfya")

ACCOUNT_CAPABILITY(kCanOfferExtendedChromeSyncPromosCapabilityName,
                   CAN_OFFER_EXTENDED_CHROME_SYNC_PROMOS_CAPABILITY_NAME,
                   "accountcapabilities/gi2tklldmfya")

ACCOUNT_CAPABILITY(kCanRunChromePrivacySandboxTrialsCapabilityName,
                   CAN_RUN_CHROME_PRIVACY_SANDBOX_TRIALS_CAPABILITY_NAME,
                   "accountcapabilities/gu2dqlldmfya")

ACCOUNT_CAPABILITY(kCanStopParentalSupervisionCapabilityName,
                   CAN_STOP_PARENTAL_SUPERVISION_CAPABILITY_NAME,
                   "accountcapabilities/guzdslldmfya")
