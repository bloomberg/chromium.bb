// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/window_finder.h"

#include "ash/wm/window_finder.h"
#include "ui/aura/window.h"

namespace {

// The class to be used by ash to find an eligible chrome window that we can
// attach the dragged tabs into.
class AshWindowFinder : public WindowFinder {
 public:
  AshWindowFinder() = default;

  gfx::NativeWindow GetLocalProcessWindowAtPoint(
      const gfx::Point& screen_point,
      const std::set<gfx::NativeWindow>& ignore) override {
    return ash::wm::GetTopmostWindowAtPoint(screen_point, ignore, nullptr);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AshWindowFinder);
};

}  // namespace

std::unique_ptr<WindowFinder> WindowFinder::Create(
    TabDragController::EventSource source,
    gfx::NativeWindow window) {
  return std::make_unique<AshWindowFinder>();
}

gfx::NativeWindow WindowFinder::GetLocalProcessWindowAtPoint(
    const gfx::Point& screen_point,
    const std::set<gfx::NativeWindow>& ignore) {
  NOTREACHED();
  return nullptr;
}
