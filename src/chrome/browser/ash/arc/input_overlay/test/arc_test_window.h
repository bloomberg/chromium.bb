// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_TEST_ARC_TEST_WINDOW_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_TEST_ARC_TEST_WINDOW_H_

#include "components/exo/shell_surface_util.h"
#include "components/exo/surface.h"
#include "components/exo/test/exo_test_helper.h"

namespace arc {
namespace input_overlay {
namespace test {

// ArcTestWindow creates window with exo/surface.
class ArcTestWindow {
 public:
  ArcTestWindow(exo::test::ExoTestHelper* helper,
                aura::Window* root,
                const std::string& package_name);
  ArcTestWindow(const ArcTestWindow&) = delete;
  ArcTestWindow& operator=(const ArcTestWindow&) = delete;
  ~ArcTestWindow();

  aura::Window* GetWindow();
  void SetMinimized();
  // Set bounds in |display|. |bounds| is the local bounds in the display.
  void SetBounds(display::Display& display, gfx::Rect bounds);

 private:
  std::unique_ptr<exo::Buffer> buffer_;
  std::unique_ptr<exo::Surface> surface_;
  std::unique_ptr<exo::ClientControlledShellSurface> shell_surface_;
};

}  // namespace test
}  // namespace input_overlay
}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_TEST_ARC_TEST_WINDOW_H_
