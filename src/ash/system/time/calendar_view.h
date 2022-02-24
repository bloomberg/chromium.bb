// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TIME_CALENDAR_VIEW_H_
#define ASH_SYSTEM_TIME_CALENDAR_VIEW_H_

#include <string>

#include "ash/ash_export.h"
#include "ash/system/time/calendar_model.h"
#include "ash/system/time/calendar_view_controller.h"
#include "ash/system/tray/tray_detailed_view.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "base/callback_list.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "base/timer/timer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/view.h"

namespace ui {
class Event;
}  // namespace ui

namespace views {

class Label;

}  // namespace views

namespace ash {

class CalendarEventListView;
class CalendarMonthView;
class IconButton;

// The header of the calendar view, which shows the current month and year.
class CalendarHeaderView : public views::View {
 public:
  CalendarHeaderView(const std::u16string& month, const std::u16string& year);
  CalendarHeaderView(const CalendarHeaderView& other) = delete;
  CalendarHeaderView& operator=(const CalendarHeaderView& other) = delete;
  ~CalendarHeaderView() override;

  // views::View:
  void OnThemeChanged() override;

  // Updates the month and year labels.
  void UpdateHeaders(const std::u16string& month, const std::u16string& year);

 private:
  friend class CalendarViewTest;
  friend class CalendarViewAnimationTest;

  // The main header which shows the month name.
  views::Label* const header_;

  // The year header which follows the `header_`.
  views::Label* const header_year_;
};

// This view displays a scrollable calendar.
class ASH_EXPORT CalendarView : public CalendarModel::Observer,
                                public CalendarViewController::Observer,
                                public TrayDetailedView,
                                public views::ViewObserver {
 public:
  METADATA_HEADER(CalendarView);

  CalendarView(DetailedViewDelegate* delegate,
               UnifiedSystemTrayController* controller);
  CalendarView(const CalendarView& other) = delete;
  CalendarView& operator=(const CalendarView& other) = delete;
  ~CalendarView() override;

  void Init();

  // CalendarModel::Observer:
  void OnEventsFetched(const google_apis::calendar::EventList* events) override;

  // CalendarViewController::Observer:
  void OnMonthChanged(const base::Time::Exploded current_month) override;
  void OpenEventList() override;
  void CloseEventList() override;

  // views::ViewObserver:
  void OnViewBoundsChanged(views::View* observed_view) override;
  void OnViewFocused(View* observed_view) override;

  // views::View:
  void OnEvent(ui::Event* event) override;

  // TrayDetailedView:
  void CreateExtraTitleRowButtons() override;
  views::Button* CreateInfoButton(views::Button::PressedCallback callback,
                                  int info_accessible_name_id) override;

  CalendarViewController* calendar_view_controller() {
    return calendar_view_controller_.get();
  }

 private:
  // The header of each month view which shows the month's name. If the year of
  // this month is not the same as the current month, the year is also shown in
  // this view.
  class MonthHeaderLabelView;

  // Content view of calendar's scroll view, used for metrics recording.
  // TODO(crbug.com/1297376): Add unit tests for metrics recording.
  class ScrollContentsView : public views::View {
   public:
    explicit ScrollContentsView(CalendarViewController* controller);
    ScrollContentsView(const ScrollContentsView& other) = delete;
    ScrollContentsView& operator=(const ScrollContentsView& other) = delete;
    ~ScrollContentsView() override = default;

    // Update the value of current month based on the controller.
    void OnMonthChanged();

    // views::View:
    void OnEvent(ui::Event* event) override;

    // Called when a stylus touch event is triggered.
    void OnStylusEvent(const ui::TouchEvent& event);

   private:
    // Used as a Shell pre-target handler to notify the owner of stylus events.
    class StylusEventHandler : public ui::EventHandler {
     public:
      explicit StylusEventHandler(ScrollContentsView* content_view);
      StylusEventHandler(const StylusEventHandler&) = delete;
      StylusEventHandler& operator=(const StylusEventHandler&) = delete;
      ~StylusEventHandler() override;

      // ui::EventHandler:
      void OnTouchEvent(ui::TouchEvent* event) override;

     private:
      ScrollContentsView* content_view_;
    };

    CalendarViewController* const controller_;
    StylusEventHandler stylus_event_handler_;

    // Since we only record metrics once when we scroll through a particular
    // month. This keeps track the current month in display that we have already
    // recorded metrics.
    std::u16string current_month_;
  };

  // The types to create the `MonthHeaderLabelView` which are in corresponding
  // to the 3 months: `previous_month_`, `current_month_` and `next_month_`.
  enum LabelType { PREVIOUS, CURRENT, NEXT };

  friend class CalendarViewTest;
  friend class CalendarViewAnimationTest;

  // Assigns month views and labels based on the current date on screen.
  void SetMonthViews();

  // Returns the current month first row position.
  int PositionOfCurrentMonth() const;

  // Returns the today's row position.
  int PositionOfToday() const;

  // Returns the selected date's row position.
  int PositionOfSelectedDate() const;

  // Adds a month label.
  views::View* AddLabelWithId(LabelType type, bool add_at_front = false);

  // Adds a `CalendarMonthView`.
  CalendarMonthView* AddMonth(base::Time month_first_date,
                              bool add_at_front = false);

  // Deletes the current next month and add a new month at the top of the
  // `content_view_`.
  void ScrollUpOneMonth();

  // Deletes the current previous month and adds a new month at the bottom of
  // the `content_view_`.
  void ScrollDownOneMonth();

  // Scrolls up or down one month then auto scrolls to the current month's first
  // row.
  void ScrollOneMonthAndAutoScroll(bool scroll_up);

  // Shows the scrolling animation then scrolls one month then auto scroll to
  // the current month's first row.
  void ScrollOneMonthWithAnimation(bool scroll_up);

  // Scrolls up/down one row based on `scroll_up`.
  void ScrollOneRowWithAnimation(bool scroll_up);

  // Sets opacity for header and content view (which contains previous, current
  // and next month with their labels),
  void SetHeaderAndContentViewOpacity(float opacity);

  // Enables or disables `should_months_animate_` and `scroll_view_` vertical
  // scroll bar mode.
  void SetShouldMonthsAnimateAndScrollEnabled(bool enabled);

  // Fades out on-screen month, sets date to today by calling `ResetToToday` and
  // fades in updated views after.
  void ResetToTodayWithAnimation();

  // Removes on-screen month and adds today's date month and label views without
  // animation.
  void ResetToToday();

  // Fades in current month.
  void FadeInCurrentMonth();

  // Updates the `header_`'s month and year to the current month and year.
  void UpdateHeaders();

  // Resets the `header_`'s opacity and position. Also resets
  // `scrolling_settled_timer_` and `header_animation_restart_timer_`.
  void RestoreHeadersStatus();

  // Resets the the month views' opacity and position. In case the animation is
  // aborted in the middle and the view's are not in the original status.
  void RestoreMonthStatus();

  // Auto scrolls to today. If the view is big enough we scroll to the first row
  // of today's month, otherwise we scroll to the position of today's row.
  void ScrollToToday();

  // If currently focusing on any date cell.
  bool IsDateCellViewFocused();

  // If focusing on `CalendarDateCellView` is interrupted (by scrolling or by
  // today's button), resets the content view's `FocusBehavior` to `ALWAYS`.
  void MaybeResetContentViewFocusBehavior();

  // We only fetch events after we've "settled" on the current on-screen month.
  void OnScrollingSettledTimerFired();

  // ScrollView callback.
  void OnContentsScrolled();

  // Callback passed to `up_button_` and `down_button_`, activated on button
  // activation.
  void OnMonthArrowButtonActivated(bool up, const ui::Event& event);

  // Adjusts the Chrome Vox box position for date cells in the scroll view.
  void AdjustDateCellVoxBounds();

  // Handles the position and status of `event_list_view_` and other views after
  // the opening event list animation or closing event list animation. Such as
  // restoring the position of them, re-enabling animation and etc.
  void OnOpenEventListAnimationComplete();
  void OnCloseEventListAnimationComplete();

  // Unowned.
  UnifiedSystemTrayController* controller_;

  std::unique_ptr<CalendarViewController> calendar_view_controller_;

  // Setters for animation flags.
  void set_should_header_animate(bool should_animate) {
    should_header_animate_ = should_animate;
  }
  void set_should_months_animate(bool should_animate) {
    should_months_animate_ = should_animate;
  }

  // Reset `scrolling_settled_timer_`.
  void reset_scrolling_settled_timer() { scrolling_settled_timer_.Reset(); }

  // The content of the `scroll_view_`, which carries months and month labels.
  // Owned by `CalendarView`.
  ScrollContentsView* content_view_ = nullptr;

  // The following is owned by `CalendarView`.
  views::ScrollView* scroll_view_ = nullptr;
  views::View* current_label_ = nullptr;
  views::View* previous_label_ = nullptr;
  views::View* next_label_ = nullptr;
  CalendarMonthView* previous_month_ = nullptr;
  CalendarMonthView* current_month_ = nullptr;
  CalendarMonthView* next_month_ = nullptr;
  CalendarHeaderView* header_ = nullptr;
  views::Button* reset_to_today_button_ = nullptr;
  views::Button* settings_button_ = nullptr;
  IconButton* up_button_ = nullptr;
  IconButton* down_button_ = nullptr;
  CalendarEventListView* event_list_view_ = nullptr;

  // If it `is_resetting_scroll_`, we don't calculate the scroll position and we
  // don't need to check if we need to update the month or not.
  bool is_resetting_scroll_ = false;

  // It's true if the header should animate, but false when it is currently
  // animating, or header changing from mouse scroll (not from the buttons) or
  // cooling down from the last animation.
  bool should_header_animate_ = true;

  // It's true if the month views should animate, but false when it is currently
  // animating, or cooling down from the last animation.
  bool should_months_animate_ = true;

  // This is used to define the animation directions for updating the header and
  // month views.
  bool is_scrolling_up_ = true;

  // Whether the Calendar View is scrolling.
  bool is_calendar_view_scrolling_ = false;

  // Timer that fires when we've "settled" on, i.e. finished scrolling to, a
  // currently-visible month
  base::RetainingOneShotTimer scrolling_settled_timer_;

  // Timers that enable the updating month/header animations. When the month
  // keeps getting changed, the animation will be disabled and the cool-down
  // duration is `kAnimationDisablingTimeout` ms to enable the next animation.
  base::RetainingOneShotTimer header_animation_restart_timer_;
  base::RetainingOneShotTimer months_animation_restart_timer_;

  base::CallbackListSubscription on_contents_scrolled_subscription_;
  base::ScopedObservation<CalendarModel, CalendarModel::Observer>
      scoped_calendar_model_observer_{this};
  base::ScopedObservation<CalendarViewController,
                          CalendarViewController::Observer>
      scoped_calendar_view_controller_observer_{this};
  base::ScopedMultiSourceObservation<views::View, views::ViewObserver>
      scoped_view_observer_{this};

  base::WeakPtrFactory<CalendarView> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_TIME_CALENDAR_VIEW_H_
