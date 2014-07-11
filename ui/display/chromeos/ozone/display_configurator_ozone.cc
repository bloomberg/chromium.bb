// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/chromeos/display_configurator.h"

#include "ui/display/chromeos/touchscreen_delegate_impl.h"
#include "ui/display/types/chromeos/native_display_delegate.h"
#include "ui/display/types/chromeos/touchscreen_device_manager.h"
#include "ui/ozone/public/ozone_platform.h"

namespace ui {

scoped_ptr<NativeDisplayDelegate>
DisplayConfigurator::CreatePlatformNativeDisplayDelegate() {
  return ui::OzonePlatform::GetInstance()->CreateNativeDisplayDelegate();
}

scoped_ptr<DisplayConfigurator::TouchscreenDelegate>
DisplayConfigurator::CreatePlatformTouchscreenDelegate() {
  return scoped_ptr<TouchscreenDelegate>(new TouchscreenDelegateImpl(
      ui::OzonePlatform::GetInstance()->CreateTouchscreenDeviceManager()));
}

}  // namespace ui
