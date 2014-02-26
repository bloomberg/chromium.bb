// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/gesture_detection/gesture_config_helper.h"

namespace ui {

GestureDetector::Config DefaultGestureDetectorConfig() {
  return GestureDetector::Config();
}

ScaleGestureDetector::Config DefaultScaleGestureDetectorConfig() {
  return ScaleGestureDetector::Config();
}

SnapScrollController::Config DefaultSnapScrollControllerConfig() {
  return SnapScrollController::Config();
}

GestureProvider::Config DefaultGestureProviderConfig() {
  return GestureProvider::Config();
}

}  // namespace ui
