// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_DISPLAY_PLATFORM_SCREEN_IMPL_OZONE_H_
#define SERVICES_UI_DISPLAY_PLATFORM_SCREEN_IMPL_OZONE_H_

#include <stdint.h>

#include <set>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "services/ui/display/platform_screen.h"
#include "ui/display/chromeos/display_configurator.h"
#include "ui/display/display.h"

namespace display {

// PlatformScreenImplOzone provides the necessary functionality to configure all
// attached physical displays on the ozone platform.
class PlatformScreenImplOzone : public PlatformScreen,
                                public ui::DisplayConfigurator::Observer {
 public:
  PlatformScreenImplOzone();
  ~PlatformScreenImplOzone() override;

 private:
  // PlatformScreen:
  void Init(PlatformScreenDelegate* delegate) override;
  int64_t GetPrimaryDisplayId() const override;

  // ui::DisplayConfigurator::Observer:
  void OnDisplayModeChanged(
      const ui::DisplayConfigurator::DisplayStateList& displays) override;
  void OnDisplayModeChangeFailed(
      const ui::DisplayConfigurator::DisplayStateList& displays,
      ui::MultipleDisplayState failed_new_state) override;

  PlatformScreenDelegate* delegate_ = nullptr;
  ui::DisplayConfigurator display_configurator_;

  // TODO(kylechar): These values can/should be replaced by DisplayLayout.
  int64_t primary_display_id_ = display::Display::kInvalidDisplayID;
  std::set<uint64_t> displays_;
  gfx::Point next_display_origin_;

  DISALLOW_COPY_AND_ASSIGN(PlatformScreenImplOzone);
};

}  // namespace display

#endif  // SERVICES_UI_DISPLAY_PLATFORM_SCREEN_IMPL_OZONE_H_
