// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DRAG_DROP_TAB_DRAG_DROP_DELEGATE_H_
#define ASH_DRAG_DROP_TAB_DRAG_DROP_DELEGATE_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ui/gfx/geometry/point.h"

namespace aura {
class Window;
}

namespace ui {
class OSExchangeData;
}

namespace ash {

class SplitViewDragIndicators;
class TabletModeBrowserWindowDragSessionWindowsHider;

// Provides special handling for Chrome tab drags on behalf of
// DragDropController. This must be created at the beginning of a tab drag and
// destroyed at the end.
class ASH_EXPORT TabDragDropDelegate {
 public:
  // Determines whether |drag_data| indicates a tab drag from a WebUI tab strip
  // (or simply returns false if the integration is disabled).
  static bool IsChromeTabDrag(const ui::OSExchangeData& drag_data);

  // Returns whether a tab from |window| is actively being dragged.
  static bool IsSourceWindowForDrag(const aura::Window* window);

  // |root_window| is the root window from which the drag originated. The drag
  // is expected to stay in |root_window|. |source_window| is the Chrome window
  // the tab was dragged from. |start_location_in_screen| is the location of
  // the touch or click that started the drag.
  TabDragDropDelegate(aura::Window* root_window,
                      aura::Window* source_window,
                      const gfx::Point& start_location_in_screen);
  ~TabDragDropDelegate();

  TabDragDropDelegate(const TabDragDropDelegate&) = delete;
  TabDragDropDelegate& operator=(const TabDragDropDelegate&) = delete;

  const aura::Window* root_window() const { return root_window_; }

  // Must be called on every drag update.
  void DragUpdate(const gfx::Point& location_in_screen);

  // Must be called on drop if it was not accepted by the drop target. After
  // calling this, this delegate must not be used.
  void Drop(const gfx::Point& location_in_screen,
            const ui::OSExchangeData& drop_data);

 private:
  // Scales or transforms the source window if appropriate for this drag.
  // |candidate_snap_position| is where the dragged tab will be snapped
  // if dropped immediately.
  void UpdateSourceWindowBoundsIfNecessary(
      SplitViewController::SnapPosition candidate_snap_position);

  // Puts the source window back into its original position.
  void RestoreSourceWindowBounds();

  aura::Window* const root_window_;
  aura::Window* const source_window_;
  const gfx::Point start_location_in_screen_;

  std::unique_ptr<SplitViewDragIndicators> split_view_drag_indicators_;
  std::unique_ptr<TabletModeBrowserWindowDragSessionWindowsHider>
      windows_hider_;
};

}  // namespace ash

#endif  // ASH_DRAG_DROP_TAB_DRAG_DROP_DELEGATE_H_
