// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_TABLET_MODE_TABLET_MODE_BROWSER_WINDOW_DRAG_DELEGATE_H_
#define ASH_WM_TABLET_MODE_TABLET_MODE_BROWSER_WINDOW_DRAG_DELEGATE_H_

#include "ash/wm/tablet_mode/tablet_mode_window_drag_delegate.h"
#include "base/macros.h"

namespace views {
class Widget;
}  // namespace views

namespace ash {

// The drag delegate for browser windows. It not only includes the logic in
// TabletModeWindowDragDelegate, but also has special logic for browser windows,
// e.g., scales the source window, shows/hides the blurred background, etc.
class TabletModeBrowserWindowDragDelegate
    : public TabletModeWindowDragDelegate {
 public:
  TabletModeBrowserWindowDragDelegate();
  ~TabletModeBrowserWindowDragDelegate() override;

 private:
  class WindowsHider;

  // TabletModeWindowDragDelegate:
  void PrepareForDraggedWindow(const gfx::Point& location_in_screen) override;
  void UpdateForDraggedWindow(const gfx::Point& location_in_screen) override;
  void EndingForDraggedWindow(
      wm::WmToplevelWindowEventHandler::DragResult result,
      const gfx::Point& location_in_screen) override;
  bool ShouldOpenOverviewWhenDragStarts() override;

  // Scales down the source window if the dragged window is dragged past the
  // |kIndicatorThresholdRatio| threshold and restores it if the dragged window
  // is dragged back toward the top of the screen. |location_in_screen| is the
  // current drag location for the dragged window.
  void UpdateSourceWindow(const gfx::Point& location_in_screen);

  // Shows/Hides/Destroies the scrim widget |scrim_| based on the current
  // location |location_in_screen|.
  void UpdateScrim(const gfx::Point& location_in_screen);

  // Shows the scrim with the specified opacity, blur and expected bounds.
  void ShowScrim(float opacity, float blur, const gfx::Rect& bounds_in_screen);

  // A widget placed below the current dragged window to show the blurred or
  // transparent background and to prevent the dragged window merge into any
  // browser window beneath it during dragging.
  std::unique_ptr<views::Widget> scrim_;

  // It's used to hide all visible windows if the source window needs to be
  // scaled up/down during dragging a tab out of the source window. It also
  // hides the home launcher if home launcher is enabled, blurs and darkens the
  // background upon its creation. All of these will be restored upon its
  // destruction.
  std::unique_ptr<WindowsHider> windows_hider_;

  DISALLOW_COPY_AND_ASSIGN(TabletModeBrowserWindowDragDelegate);
};

}  // namespace ash

#endif  // ASH_WM_TABLET_MODE_TABLET_MODE_BROWSER_WINDOW_DRAG_DELEGATE_H_
