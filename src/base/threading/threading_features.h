// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_THREADING_THREADING_FEATURES_H_
#define BASE_THREADING_THREADING_FEATURES_H_

#include "base/base_export.h"
#include "build/build_config.h"

namespace base {

struct Feature;

#if defined(OS_APPLE)
extern const BASE_EXPORT Feature kOptimizedRealtimeThreadingMac;
extern const BASE_EXPORT FeatureParam<bool>
    kOptimizedRealtimeThreadingMacPreemptible;
extern const BASE_EXPORT FeatureParam<double>
    kOptimizedRealtimeThreadingMacBusy;
extern const BASE_EXPORT FeatureParam<double>
    kOptimizedRealtimeThreadingMacBusyLimit;
#endif

}  // namespace base

#endif  // BASE_THREADING_THREADING_FEATURES_H_
