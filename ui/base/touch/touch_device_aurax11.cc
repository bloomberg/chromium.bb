// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/touch/touch_device.h"
#include "ui/events/devices/x11/touch_factory_x11.h"

namespace ui {

bool IsTouchDevicePresent() {
  return ui::TouchFactory::GetInstance()->IsTouchDevicePresent();
}

int MaxTouchPoints() {
  return ui::TouchFactory::GetInstance()->GetMaxTouchPoints();
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
