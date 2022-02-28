// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NET_NETWORK_HEALTH_NETWORK_HEALTH_CONSTANTS_H_
#define CHROME_BROWSER_ASH_NET_NETWORK_HEALTH_NETWORK_HEALTH_CONSTANTS_H_

#include "base/time/time.h"

namespace chromeos {
namespace network_health {

// The rate in seconds at which to sample all network's signal strengths.
constexpr base::TimeDelta kSignalStrengthSampleRate = base::Seconds(5);

// This value represents the size of the sampling window for all network's
// signal strength. Samples older than this duration are discarded.
constexpr base::TimeDelta kSignalStrengthSampleWindow = base::Minutes(15);

}  // namespace network_health
}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_NET_NETWORK_HEALTH_NETWORK_HEALTH_CONSTANTS_H_
