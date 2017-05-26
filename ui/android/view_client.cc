// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/android/view_client.h"

namespace ui {

bool ViewClient::OnTouchEvent(const MotionEventAndroid& event,
                              bool for_touch_handle) {
  return false;
}

bool ViewClient::OnMouseEvent(const MotionEventAndroid& event) {
  return false;
}

bool ViewClient::OnMouseWheelEvent(const MotionEventAndroid& event) {
  return false;
}

bool ViewClient::OnDragEvent(const DragEventAndroid& event) {
  return false;
}

void ViewClient::OnPhysicalBackingSizeChanged() {}

}  // namespace ui
