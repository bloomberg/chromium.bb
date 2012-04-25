// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/win/metro.h"

#include <string>
#include <windows.h>

namespace ui {

bool IsInMetroMode() {
  const char* kMetroModeEnvVar = "CHROME_METRO_DLL";
  char buffer[2];
  if (!::GetEnvironmentVariableA(kMetroModeEnvVar, buffer, arraysize(buffer)))
    return false;
  return buffer == std::string("1");
}

}  // namespace ui
