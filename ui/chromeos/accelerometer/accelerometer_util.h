// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_CHROMEOS_ACCELEROMETER_ACCELEROMETER_UTIL_H_
#define UI_CHROMEOS_ACCELEROMETER_ACCELEROMETER_UTIL_H_

#include "chromeos/accelerometer/accelerometer_types.h"
#include "ui/chromeos/ui_chromeos_export.h"
#include "ui/gfx/geometry/vector3d_f.h"

namespace ui {

// Converts the acceleration data in |reading| into a gfx::Vector3dF.
UI_CHROMEOS_EXPORT const gfx::Vector3dF ConvertAccelerometerReadingToVector3dF(
    const chromeos::AccelerometerReading& reading);

// A reading is considered stable if its deviation from gravity is small. This
// returns false if the deviation is too high, or if |source| is not present
// in the update.
UI_CHROMEOS_EXPORT bool IsAccelerometerReadingStable(
    const chromeos::AccelerometerUpdate& update,
    chromeos::AccelerometerSource source);

}  // namespace ui

#endif  // UI_CHROMEOS_ACCELEROMETER_ACCELEROMETER_UTIL_H_
