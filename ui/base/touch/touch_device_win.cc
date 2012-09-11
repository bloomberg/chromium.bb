// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/touch/touch_device_win.h"
#include "base/win/windows_version.h"
#include <windows.h>

namespace ui {

bool IsTouchDevicePresent() {
  // Docs: http://msdn.microsoft.com/en-us/library/dd371581(VS.85).aspx
  return (::base::win::GetVersion() >= ::base::win::VERSION_WIN7) &&
      (::GetSystemMetrics(SM_DIGITIZER) > 0);
}

}  // namespace ui
