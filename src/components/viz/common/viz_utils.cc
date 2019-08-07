// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/viz_utils.h"

#include "base/system/sys_info.h"
#include "build/build_config.h"

namespace viz {

bool PreferRGB565ResourcesForDisplay() {
#if defined(OS_ANDROID)
  return base::SysInfo::AmountOfPhysicalMemoryMB() <= 512;
#endif
  return false;
}

}  // namespace viz
