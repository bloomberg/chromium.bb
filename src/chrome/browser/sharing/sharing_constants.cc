// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sharing_constants.h"

const char kFCMScope[] = "GCM";

const char kSharingFCMAppID[] = "com.google.chrome.sharing.fcm";

const constexpr base::TimeDelta kAckTimeToLive =
    base::TimeDelta::FromMinutes(30);

const constexpr base::TimeDelta kDeviceExpiration =
    base::TimeDelta::FromDays(2);

const constexpr base::TimeDelta kRegistrationExpiration =
    base::TimeDelta::FromDays(1);

const constexpr base::TimeDelta kSendMessageTimeout =
    base::TimeDelta::FromSeconds(15);

const constexpr net::BackoffEntry::Policy kRetryBackoffPolicy = {
    // Number of initial errors (in sequence) to ignore before applying
    // exponential back-off rules.
    0,

    // Initial delay.  The interpretation of this value depends on
    // always_use_initial_delay.  It's either how long we wait between
    // requests before backoff starts, or how much we delay the first request
    // after backoff starts.
    5 * 60 * 1000,

    // Factor by which the waiting time will be multiplied.
    2.0,

    // Fuzzing percentage. ex: 10% will spread requests randomly
    // between 90%-100% of the calculated time.
    0.1,

    // Maximum amount of time we are willing to delay our request, -1
    // for no maximum.
    -1,

    // Time to keep an entry from being discarded even when it
    // has no significant state, -1 to never discard.
    -1,

    // If true, we always use a delay of initial_delay_ms, even before
    // we've seen num_errors_to_ignore errors.  Otherwise, initial_delay_ms
    // is the first delay once we start exponential backoff.
    false,
};
