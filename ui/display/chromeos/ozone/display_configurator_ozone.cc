// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/chromeos/display_configurator.h"

#include "ui/display/types/native_display_delegate.h"
#include "ui/ozone/public/ozone_platform.h"

namespace ui {

scoped_ptr<NativeDisplayDelegate>
DisplayConfigurator::CreatePlatformNativeDisplayDelegate() {
  return ui::OzonePlatform::GetInstance()->CreateNativeDisplayDelegate();
}

}  // namespace ui
