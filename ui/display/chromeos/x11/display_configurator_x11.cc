// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/chromeos/display_configurator.h"

#include "ui/display/chromeos/x11/native_display_delegate_x11.h"

namespace ui {

scoped_ptr<NativeDisplayDelegate>
DisplayConfigurator::CreatePlatformNativeDisplayDelegate() {
  return scoped_ptr<NativeDisplayDelegate>(new NativeDisplayDelegateX11());
}

}  // namespace ui
