// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/screen.h"

#include "base/logging.h"

namespace gfx {

// static
gfx::Size Screen::GetPrimaryMonitorSize() {
  NOTIMPLEMENTED() << "crbug.com/117839 tracks implementation";
  return gfx::Size(1, 1);
}

// static
int Screen::GetNumMonitors() {
  return 1;
}

}  // namespace gfx
