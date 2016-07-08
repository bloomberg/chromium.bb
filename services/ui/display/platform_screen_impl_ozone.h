// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_DISPLAY_PLATFORM_SCREEN_IMPL_OZONE_H_
#define SERVICES_UI_DISPLAY_PLATFORM_SCREEN_IMPL_OZONE_H_

#include <stdint.h>

#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "services/ui/display/platform_screen.h"
#include "ui/display/chromeos/display_configurator.h"

namespace ui {
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
  void Init() override;  // Must not be called until after the ozone platform is
                         // initialized.
  void ConfigurePhysicalDisplay(
      const ConfiguredDisplayCallback& callback) override;

  // ui::DisplayConfigurator::Observer:
  void OnDisplayModeChanged(
      const ui::DisplayConfigurator::DisplayStateList& displays) override;
  void OnDisplayModeChangeFailed(
      const ui::DisplayConfigurator::DisplayStateList& displays,
      MultipleDisplayState failed_new_state) override;

  ui::DisplayConfigurator display_configurator_;

  // Callback to called when new displays are configured.
  ConfiguredDisplayCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(PlatformScreenImplOzone);
};

}  // namespace display
}  // namespace ui

#endif  // SERVICES_UI_DISPLAY_PLATFORM_SCREEN_IMPL_OZONE_H_
