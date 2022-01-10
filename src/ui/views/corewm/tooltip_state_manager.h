// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_COREWM_TOOLTIP_STATE_MANAGER_H_
#define UI_VIEWS_COREWM_TOOLTIP_STATE_MANAGER_H_

#include <map>
#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/corewm/tooltip.h"
#include "ui/views/corewm/tooltip_controller.h"
#include "ui/views/views_export.h"

namespace aura {
class Window;
}

namespace views {
namespace corewm {

namespace test {
class TooltipControllerTestHelper;
}  // namespace test

// TooltipStateManager separates the state handling from the events handling of
// the TooltipController. It is in charge of updating the tooltip state and
// keeping track of it.
class VIEWS_EXPORT TooltipStateManager {
 public:
  explicit TooltipStateManager(std::unique_ptr<Tooltip> tooltip);
  TooltipStateManager(const TooltipStateManager&) = delete;
  TooltipStateManager& operator=(const TooltipStateManager&) = delete;
  ~TooltipStateManager();

  int GetMaxWidth(const gfx::Point& location) const;

  // Hide the tooltip, clear timers, and reset controller states.
  void HideAndReset();

  // Returns true if there's a timer that'll show the tooltip running.
  bool IsWillShowTooltipTimerRunning() const {
    return will_show_tooltip_timer_.IsRunning();
  }

  bool IsVisible() const { return tooltip_->IsVisible(); }

  // Update the tooltip state attributes and start timer to show the tooltip. If
  // |hide_timeout| is greater than 0, set a timer to hide it after a specific
  // delay. Otherwise, show indefinitely.
  void Show(aura::Window* window,
            const std::u16string& tooltip_text,
            const gfx::Point& position,
            TooltipTrigger trigger,
            const base::TimeDelta hide_delay);

  void StopWillHideTooltipTimer();
  void StopWillShowTooltipTimer();

  // Returns the |tooltip_id_|, which corresponds to the pointer of the view on
  // which the tooltip was last added.
  const void* tooltip_id() const { return tooltip_id_; }

  // Returns the |tooltip_text_|, which corresponds to the last value the
  // tooltip got updated to.
  const std::u16string& tooltip_text() const { return tooltip_text_; }

  const aura::Window* tooltip_parent_window() const {
    return tooltip_parent_window_;
  }

  TooltipTrigger tooltip_trigger() const { return tooltip_trigger_; }

  // Update the |position_| if we're about to show the tooltip. This is to
  // ensure that the tooltip's position is aligned with either the latest cursor
  // location for a cursor triggered tooltip or the most recent position
  // received for a keyboard triggered tooltip.
  void UpdatePositionIfNeeded(const gfx::Point& position,
                              TooltipTrigger trigger);

 private:
  friend class test::TooltipControllerTestHelper;

  // For tests only.
  bool IsWillHideTooltipTimerRunningForTesting() const {
    return will_hide_tooltip_timer_.IsRunning();
  }

  // Calling this will enable/disable the delay that prevents the tooltip from
  // being displayed right away.
  void SetTooltipShowDelayedForTesting(bool is_delayed);

  // Called once the |will_show_timer_| fires to show the tooltip.
  void ShowNow(const std::u16string& trimmed_text,
               const base::TimeDelta hide_delay);

  // Start the show timer to show the tooltip.
  void StartWillShowTooltipTimer(const std::u16string& trimmed_text,
                                 const base::TimeDelta hide_delay);

  // The current position of the tooltip. This position is relative to the
  // |tooltip_window_| and in that window's coordinate space.
  gfx::Point position_;

  std::unique_ptr<Tooltip> tooltip_;

  // The pointer to the view for which the tooltip is set.
  raw_ptr<const void> tooltip_id_ = nullptr;

  // The text value used at the last tooltip update.
  std::u16string tooltip_text_;

  // The window on which the tooltip is added.
  raw_ptr<aura::Window> tooltip_parent_window_ = nullptr;

  TooltipTrigger tooltip_trigger_ = TooltipTrigger::kCursor;

  // Two timers for the tooltip: one to hide an on-screen tooltip after a delay,
  // and one to display the tooltip when the timer fires.
  base::OneShotTimer will_hide_tooltip_timer_;
  base::OneShotTimer will_show_tooltip_timer_;

  // The delay after which the tooltip shows up. It is only modified in the unit
  // tests.
  base::TimeDelta tooltip_show_delay_;

  // WeakPtrFactory to use for callbacks.
  base::WeakPtrFactory<TooltipStateManager> weak_factory_{this};
};

}  // namespace corewm
}  // namespace views

#endif  // UI_VIEWS_COREWM_TOOLTIP_STATE_MANAGER_H_
