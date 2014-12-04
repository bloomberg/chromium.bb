// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/touch/touch_device.h"

namespace ui {

bool IsTouchDevicePresent() {
  // TODO(sadrul@chromium.org): Support evdev hotplugging.
  return true;
}

int MaxTouchPoints() {
  // Hard-code this to 11 until we have a real implementation.
  return 11;
}

int GetAvailablePointerTypes() {
  // TODO(mustaq): Replace the stub below
  return POINTER_TYPE_NONE;
}

PointerType GetPrimaryPointerType() {
  // TODO(mustaq): Replace the stub below
  return POINTER_TYPE_NONE;
}

int GetAvailableHoverTypes() {
  // TODO(mustaq): Replace the stub below
  return HOVER_TYPE_NONE;
}

HoverType GetPrimaryHoverType() {
  // TODO(mustaq): Replace the stub below
  return HOVER_TYPE_NONE;
}

}  // namespace ui
