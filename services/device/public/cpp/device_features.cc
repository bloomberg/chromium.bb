// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/device_features.h"

namespace features {

// Enables sensors based on Generic Sensor API:
// https://w3c.github.io/sensors/
const base::Feature kGenericSensor{"GenericSensor",
                                   base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace features
