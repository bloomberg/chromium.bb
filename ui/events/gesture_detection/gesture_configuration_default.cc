// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/gesture_detection/gesture_configuration.h"

#include "ui/gfx/screen.h"

namespace ui {

// Create a GestureConfigurationAura singleton instance when using Mac.
GestureConfiguration* GestureConfiguration::GetInstance() {
  return Singleton<GestureConfiguration>::get();
}

}  // namespace ui
