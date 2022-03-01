// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_APP_LIST_BUBBLE_PRESENTER_H_
#define ASH_APP_LIST_APP_LIST_BUBBLE_PRESENTER_H_

#include <stdint.h>

#include <memory>

#include "ash/ash_export.h"
#include "ash/public/cpp/shelf_types.h"
#include "base/memory/weak_ptr.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/aura/client/focus_change_observer.h"
#include "ui/display/display_observer.h"
#include "ui/views/widget/widget_observer.h"

namespace aura {
class Window;
}  // namespace aura

namespace ash {

class AppListBubbleEventFilter;
class AppListBubbleView;
class AppListControllerImpl;
enum class AppListSortOrder;

// Manages the UI for the bubble launcher used in clamshell mode. Handles
// showing and hiding the UI, as well as bounds computations. Only one bubble
// can be visible at a time, across all displays.
class ASH_EXPORT AppListBubblePresenter
    : public views::WidgetObserver,
      public aura::client::FocusChangeObserver,
      public display::DisplayObserver {
 public:
  explicit AppListBubblePresenter(AppListControllerImpl* controller);
  AppListBubblePresenter(const AppListBubblePresenter&) = delete;
  AppListBubblePresenter& operator=(const AppListBubblePresenter&) = delete;
  ~AppListBubblePresenter() override;

  // Shows the bubble on the display with `display_id`. The bubble is shown
  // asynchronously (after a delay) because the continue suggestions need to be
  // refreshed before the bubble views can be created and animated. This delay
  // is skipped in unit tests (see TestAppListClient) for convenience. Larger
  // tests (e.g. browser_tests) may need to wait for the window to open.
  void Show(int64_t display_id);

  // Shows or hides the bubble on the display with `display_id`. Returns the
  // appropriate ShelfAction to indicate whether the bubble was shown or hidden.
  ShelfAction Toggle(int64_t display_id);

  // Closes and destroys the bubble.
  void Dismiss();

  // Returns the bubble window or nullptr if it is not open.
  aura::Window* GetWindow() const;

  // Returns true if the bubble is showing on any display.
  bool IsShowing() const;

  // Returns true if the assistant page is showing.
  bool IsShowingEmbeddedAssistantUI() const;

  // Switches to the assistant page. Requires the bubble to be open.
  void ShowEmbeddedAssistantUI();

  // Called when the app list temporary sort order changes.
  void OnTemporarySortOrderChanged(
      const absl::optional<AppListSortOrder>& new_order);

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  // aura::client::FocusChangeObserver:
  void OnWindowFocused(aura::Window* gained_focus,
                       aura::Window* lost_focus) override;

  // DisplayObserver:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

  views::Widget* bubble_widget_for_test() { return bubble_widget_; }
  AppListBubbleView* bubble_view_for_test() { return bubble_view_; }

 private:
  // The code doesn't need a value for the search page.
  enum class Page { kApps, kAssistant };

  // Callback for zero state search update. Builds the bubble widget and views
  // on display `display_id` and triggers the show animation.
  void OnZeroStateSearchDone(int64_t display_id);

  // Callback for AppListBubbleEventFilter, used to notify this of presses
  // outside the bubble.
  void OnPressOutsideBubble();

  // Gets the display id for the display `bubble_widget_` is shown on, returns
  // kInvalidDisplayId if not shown.
  int64_t GetDisplayId() const;

  // Callback for the hide animation.
  void OnHideAnimationEnded();

  AppListControllerImpl* const controller_;

  // Owned by native widget.
  views::Widget* bubble_widget_ = nullptr;

  // Owned by views.
  AppListBubbleView* bubble_view_ = nullptr;

  // Whether the bubble hide animation is playing.
  bool in_hide_animation_ = false;

  // The page to show after the views are constructed.
  Page initial_page_ = Page::kApps;

  // Closes the widget when the user clicks outside of it.
  std::unique_ptr<AppListBubbleEventFilter> bubble_event_filter_;

  // Observes display configuration changes.
  display::ScopedDisplayObserver display_observer_{this};

  base::WeakPtrFactory<AppListBubblePresenter> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_APP_LIST_APP_LIST_BUBBLE_PRESENTER_H_
