// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/features.h"

namespace network {
namespace features {

const base::Feature kNetworkErrorLogging{"NetworkErrorLogging",
                                         base::FEATURE_DISABLED_BY_DEFAULT};
// Enables the network service.
const base::Feature kNetworkService{"NetworkService",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

// Out of Blink CORS
const base::Feature kOutOfBlinkCORS{"OutOfBlinkCORS",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

// Port some content::ResourceScheduler functionalities to renderer.
const base::Feature kRendererSideResourceScheduler{
    "RendererSideResourceScheduler", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kReporting{"Reporting", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace features
}  // namespace network
