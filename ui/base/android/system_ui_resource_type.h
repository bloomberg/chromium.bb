// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_ANDROID_SYSTEM_UI_RESOURCE_TYPE_H_
#define UI_BASE_ANDROID_SYSTEM_UI_RESOURCE_TYPE_H_

namespace ui {

// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.ui.base
enum SystemUIResourceType {
  OVERSCROLL_EDGE,
  OVERSCROLL_GLOW,
  OVERSCROLL_GLOW_L,
  OVERSCROLL_REFRESH_IDLE,
  OVERSCROLL_REFRESH_ACTIVE,
  SYSTEM_UI_RESOURCE_TYPE_FIRST = OVERSCROLL_EDGE,
  SYSTEM_UI_RESOURCE_TYPE_LAST = OVERSCROLL_REFRESH_ACTIVE
};

}  // namespace ui

#endif  // UI_BASE_ANDROID_SYSTEM_UI_RESOURCE_TYPE_H_
