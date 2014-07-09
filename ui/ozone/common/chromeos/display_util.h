// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_COMMON_CHROMEOS_DISPLAY_UTIL_H_
#define UI_OZONE_COMMON_CHROMEOS_DISPLAY_UTIL_H_

#include "ui/ozone/common/gpu/ozone_gpu_message_params.h"

namespace ui {

class DisplayMode;
class DisplaySnapshot;

DisplayMode_Params GetDisplayModeParams(const DisplayMode& mode);
DisplaySnapshot_Params GetDisplaySnapshotParams(
    const DisplaySnapshot& display);

}  // namespace ui

#endif  // UI_OZONE_COMMON_CHROMEOS_DISPLAY_UTIL_H_
