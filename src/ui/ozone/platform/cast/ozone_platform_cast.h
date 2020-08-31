// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_CAST_OZONE_PLATFORM_CAST_H
#define UI_OZONE_PLATFORM_CAST_OZONE_PLATFORM_CAST_H

namespace ui {

class OzonePlatform;

// Constructor hook for use in ozone_platform_list.cc
OzonePlatform* CreateOzonePlatformCast();

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_CAST_OZONE_PLATFORM_CAST_H
