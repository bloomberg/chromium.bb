// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_CYCLE_WINDOW_CYCLE_TAB_SLIDER_H_
#define ASH_WM_WINDOW_CYCLE_WINDOW_CYCLE_TAB_SLIDER_H_

#include "ash/ash_export.h"
#include "ash/wm/window_cycle/window_cycle_tab_slider_button.h"
#include "ash/wm/wm_highlight_item_border.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace gfx {
class Size;
}

namespace ash {

// A WindowCycleTabSlider containing two buttons to switch between
// all desks and current desks mode.
class ASH_EXPORT WindowCycleTabSlider : public views::View {
 public:
  METADATA_HEADER(WindowCycleTabSlider);

  WindowCycleTabSlider();
  WindowCycleTabSlider(const WindowCycleTabSlider&) = delete;
  WindowCycleTabSlider& operator=(const WindowCycleTabSlider&) = delete;
  ~WindowCycleTabSlider() override = default;

  // Sets |is_focused_| to |focus| and displays or hides the highlight on the
  // active button selector during keyboard navigation.
  void SetFocus(bool focus);

  // Updates UI when user prefs change.
  void OnModePrefsChanged();

  // views::View:
  void Layout() override;
  gfx::Size CalculatePreferredSize() const override;

  bool is_focused() const { return is_focused_; }

 private:
  friend class WindowCycleListTestApi;

  // Updates the active button selector with moving animation from the
  // currently selected button to the target button representing |per_desk|
  // mode.
  void UpdateActiveButtonSelector(bool per_desk);

  // Returns an equalized button size calculated from maximum width and height
  // of the prefer size of all buttons.
  gfx::Size GetPreferredSizeForButtons();

  // The view that acts as an active button selector to show the active button
  // background and the highlight border if applicable. It is animated during
  // mode change.
  views::View* active_button_selector_;

  // The highlight border, the focus ring, of the active button selector.
  // The border shows up when the tab slider is focused during keyboard
  // navigation.
  WmHighlightItemBorder* highlight_border_;

  // The view that contains the tab slider buttons.
  views::View* buttons_container_;

  WindowCycleTabSliderButton* all_desks_tab_slider_button_;
  WindowCycleTabSliderButton* current_desk_tab_slider_button_;

  // True if the tab slider is focused when using keyboard navigation.
  bool is_focused_ = false;
};

}  // namespace ash

#endif  // ASH_WM_WINDOW_CYCLE_WINDOW_CYCLE_TAB_SLIDER_H_
