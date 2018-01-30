// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_FEATURES_
#define SERVICES_NETWORK_PUBLIC_CPP_FEATURES_

#include "base/feature_list.h"
#include "services/network/public/cpp/features_export.h"

namespace network {
namespace features {

NETWORK_FEATURES_EXPORT extern const base::Feature kNetworkErrorLogging;
NETWORK_FEATURES_EXPORT extern const base::Feature kNetworkService;
NETWORK_FEATURES_EXPORT extern const base::Feature kOutOfBlinkCORS;
NETWORK_FEATURES_EXPORT extern const base::Feature
    kRendererSideResourceScheduler;
NETWORK_FEATURES_EXPORT extern const base::Feature kReporting;

}  // namespace features
}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_FEATURES_
