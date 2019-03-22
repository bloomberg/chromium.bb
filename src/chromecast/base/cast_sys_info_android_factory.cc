// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/chromecast_buildflags.h"
#if BUILDFLAG(IS_ANDROID_THINGS)
#include "chromecast/base/cast_sys_info_android_things.h"
#endif
#include "chromecast/base/cast_sys_info_android.h"

namespace chromecast {

// static
std::unique_ptr<CastSysInfo> CreateSysInfo() {
// Not used for Android N because N doesn't support ro.oem.*
#if BUILDFLAG(IS_ANDROID_THINGS) && !BUILDFLAG(USE_ANDROID_THINGS_N)
  return std::make_unique<CastSysInfoAndroidThings>();
#else
  return std::make_unique<CastSysInfoAndroid>();
#endif
}

}  // namespace chromecast
