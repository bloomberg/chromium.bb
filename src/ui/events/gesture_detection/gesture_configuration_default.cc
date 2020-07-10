// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/gesture_detection/gesture_configuration.h"

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "ui/display/screen.h"

namespace ui {
namespace {
class GestureConfigurationDefault : public GestureConfiguration {
 public:
  ~GestureConfigurationDefault() override {
  }

  static GestureConfigurationDefault* GetInstance() {
    return base::Singleton<GestureConfigurationDefault>::get();
  }

 private:
  GestureConfigurationDefault() {}

  friend struct base::DefaultSingletonTraits<GestureConfigurationDefault>;
  DISALLOW_COPY_AND_ASSIGN(GestureConfigurationDefault);
};

}  // namespace

// Create a GestureConfiguration singleton instance when using Mac.
GestureConfiguration* GestureConfiguration::GetPlatformSpecificInstance() {
  return GestureConfigurationDefault::GetInstance();
}

}  // namespace ui
