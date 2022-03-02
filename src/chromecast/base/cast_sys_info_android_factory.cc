// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/base/cast_sys_info_android.h"

namespace chromecast {

// static
std::unique_ptr<CastSysInfo> CreateSysInfo() {
  return std::make_unique<CastSysInfoAndroid>();
}

}  // namespace chromecast
