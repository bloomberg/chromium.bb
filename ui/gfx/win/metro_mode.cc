// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/win/metro_mode.h"

#include "base/win/windows_version.h"

namespace gfx {
namespace win {

bool ShouldUseMetroMode() {
  if (base::win::GetVersion() < base::win::VERSION_WIN8)
    return false;

  // As of now ActivateApplication fails on Windows 10 (Build 9926).
  // Until there is some clarity on special status of browser in metro mode on
  // Windows 10, we just  disable Chrome metro mode so that browser remains
  // usable. See crbug.com/470227.
  if (base::win::GetVersion() >= base::win::VERSION_WIN10)
    return false;

  return true;
}

}  // namespace win
}  // namespace gfx
