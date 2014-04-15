// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/chromeos/display_configurator.h"

#include "ui/display/chromeos/ozone/touchscreen_delegate_ozone.h"
#include "ui/display/types/chromeos/native_display_delegate.h"
#include "ui/ozone/ozone_platform.h"

namespace ui {

void DisplayConfigurator::PlatformInitialize() {
  InitializeDelegates(
      ui::OzonePlatform::GetInstance()->CreateNativeDisplayDelegate(),
      scoped_ptr<TouchscreenDelegate>(new TouchscreenDelegateOzone()));
}

}  // namespace ui
