// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/window_finder.h"

#include "base/memory/ptr_util.h"

// static
std::unique_ptr<WindowFinder> WindowFinder::Create(
    TabDragController::EventSource source,
    gfx::NativeWindow window) {
  return base::WrapUnique(new WindowFinder());
}
