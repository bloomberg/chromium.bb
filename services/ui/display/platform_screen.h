// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_DISPLAY_PLATFORM_SCREEN_H_
#define SERVICES_UI_DISPLAY_PLATFORM_SCREEN_H_

#include <stdint.h>

#include "base/callback.h"

namespace gfx {
class Rect;
}

namespace ui {
namespace display {

// PlatformScreen provides the necessary functionality to configure all
// attached physical displays.
class PlatformScreen {
 public:
  using ConfiguredDisplayCallback =
      base::Callback<void(int64_t, const gfx::Rect&)>;

  virtual ~PlatformScreen() {}

  // Creates a PlatformScreen instance.
  static std::unique_ptr<PlatformScreen> Create();

  // Initializes platform specific screen resources.
  virtual void Init() = 0;

  // ConfigurePhysicalDisplay() configures a single physical display and returns
  // its id and bounds for it via |callback|.
  virtual void ConfigurePhysicalDisplay(
      const ConfiguredDisplayCallback& callback) = 0;
};

}  // namespace display
}  // namespace ui

#endif  // SERVICES_UI_DISPLAY_PLATFORM_SCREEN_H_
