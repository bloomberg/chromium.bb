// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_OZONE_PLATFORM_LIST_H_
#define UI_OZONE_OZONE_PLATFORM_LIST_H_

namespace ui {

class OzonePlatform;

typedef OzonePlatform* (*OzonePlatformConstructor)();

struct OzonePlatformListEntry {
  const char* name;
  OzonePlatformConstructor constructor;
};

extern const OzonePlatformListEntry kOzonePlatforms[];

extern const int kOzonePlatformCount;

}  // namespace ui

#endif  // UI_OZONE_OZONE_PLATFORM_LIST_H_
