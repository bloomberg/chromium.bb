// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_DISPLAY_PLATFORM_SCREEN_IMPL_H_
#define SERVICES_UI_DISPLAY_PLATFORM_SCREEN_IMPL_H_

#include <stdint.h>

#include "base/callback.h"
#include "services/ui/display/platform_screen.h"

namespace display {

// PlatformScreenImpl provides the necessary functionality to configure all
// attached physical displays on non-ozone platforms.
class PlatformScreenImpl : public PlatformScreen {
 public:
  PlatformScreenImpl();
  ~PlatformScreenImpl() override;

 private:
  // PlatformScreen.
  void Init() override;
  void ConfigurePhysicalDisplay(
      const PlatformScreen::ConfiguredDisplayCallback& callback) override;
  int64_t GetPrimaryDisplayId() const override;

  DISALLOW_COPY_AND_ASSIGN(PlatformScreenImpl);
};

}  // namespace display

#endif  // SERVICES_UI_DISPLAY_PLATFORM_SCREEN_IMPL_H_
