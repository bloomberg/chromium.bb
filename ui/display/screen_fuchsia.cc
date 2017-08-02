// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/screen.h"

#include "base/logging.h"

namespace display {

// static
gfx::NativeWindow Screen::GetWindowForView(gfx::NativeView view) {
  // TODO(fuchsia): Stubbed during headless bringup https://crbug.com/743296.
  NOTREACHED();
  return nullptr;
}

}  // namespace display
