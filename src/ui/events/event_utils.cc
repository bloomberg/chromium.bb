// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/event_utils.h"

#include <limits>
#include <map>
#include <vector>

#include "base/check.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/time/tick_clock.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/base_event_utils.h"

namespace ui {

namespace {

int g_custom_event_types = ET_LAST;

#if defined(OS_WIN)

#define UMA_HISTOGRAM_WIN_EVENT_LATENCY_TIMES(name, sample)        \
  UMA_HISTOGRAM_CUSTOM_TIMES(name, sample,                         \
                             base::TimeDelta::FromMilliseconds(1), \
                             base::TimeDelta::FromMilliseconds(100000), 100)

class GetTickCountClock : public base::TickClock {
 public:
  GetTickCountClock() = default;
  ~GetTickCountClock() override = default;

  base::TimeTicks NowTicks() const override {
    return base::TimeTicks() +
           base::TimeDelta::FromMilliseconds(::GetTickCount());
  }
};

const base::TickClock* g_tick_count_clock = nullptr;

// Logs experimental Event.Latency.OS_WIN.* metrics to parallel
// Event.Latency.OS.*.
// TODO(crbug.com/1189656): Check that these are accurate enough to replace
// Event.Latency.OS.*.
void ComputeEventLatencyOSWin(ui::EventType event_type,
                              base::TimeTicks event_time,
                              base::TimeTicks current_time) {
  // Check that the event doesn't come from a device giving bogus timestamps.
  // On most platforms this is done inside EventTimeFromNative.
  const bool is_valid = IsValidTimebase(current_time, event_time);
  UMA_HISTOGRAM_BOOLEAN("Event.Latency.OS_WIN_IS_VALID", is_valid);
  switch (event_type) {
    case ET_KEY_PRESSED:
      UMA_HISTOGRAM_BOOLEAN("Event.Latency.OS_WIN_IS_VALID.KEY_PRESSED",
                            is_valid);
      break;
    case ET_MOUSE_PRESSED:
      UMA_HISTOGRAM_BOOLEAN("Event.Latency.OS_WIN_IS_VALID.MOUSE_PRESSED",
                            is_valid);
      break;
    case ET_TOUCH_PRESSED:
      UMA_HISTOGRAM_BOOLEAN("Event.Latency.OS_WIN_IS_VALID.TOUCH_PRESSED",
                            is_valid);
      break;
    default:
      // Caller should have filtered out unhandled events.
      NOTREACHED();
      break;
  }
  if (!is_valid)
    return;

  const base::TimeDelta delta = current_time - event_time;

  if (base::TimeTicks::IsHighResolution()) {
    UMA_HISTOGRAM_WIN_EVENT_LATENCY_TIMES("Event.Latency.OS_WIN.HIGH_RES",
                                          delta);
    switch (event_type) {
      case ET_KEY_PRESSED:
        UMA_HISTOGRAM_WIN_EVENT_LATENCY_TIMES(
            "Event.Latency.OS_WIN.HIGH_RES.KEY_PRESSED", delta);
        break;
      case ET_MOUSE_PRESSED:
        UMA_HISTOGRAM_WIN_EVENT_LATENCY_TIMES(
            "Event.Latency.OS_WIN.HIGH_RES.MOUSE_PRESSED", delta);
        break;
      case ET_TOUCH_PRESSED:
        UMA_HISTOGRAM_WIN_EVENT_LATENCY_TIMES(
            "Event.Latency.OS_WIN.HIGH_RES.TOUCH_PRESSED", delta);
        break;
      default:
        // Caller should have filtered out unhandled events.
        NOTREACHED();
        break;
    }
  } else {
    UMA_HISTOGRAM_WIN_EVENT_LATENCY_TIMES("Event.Latency.OS_WIN.LOW_RES",
                                          delta);
    switch (event_type) {
      case ET_KEY_PRESSED:
        UMA_HISTOGRAM_WIN_EVENT_LATENCY_TIMES(
            "Event.Latency.OS_WIN.LOW_RES.KEY_PRESSED", delta);
        break;
      case ET_MOUSE_PRESSED:
        UMA_HISTOGRAM_WIN_EVENT_LATENCY_TIMES(
            "Event.Latency.OS_WIN.LOW_RES.MOUSE_PRESSED", delta);
        break;
      case ET_TOUCH_PRESSED:
        UMA_HISTOGRAM_WIN_EVENT_LATENCY_TIMES(
            "Event.Latency.OS_WIN.LOW_RES.TOUCH_PRESSED", delta);
        break;
      default:
        // Caller should have filtered out unhandled events.
        NOTREACHED();
        break;
    }
  }
}

#endif  // OS_WIN

}  // namespace

std::unique_ptr<Event> EventFromNative(const PlatformEvent& native_event) {
  std::unique_ptr<Event> event;
  EventType type = EventTypeFromNative(native_event);
  switch(type) {
    case ET_KEY_PRESSED:
    case ET_KEY_RELEASED:
      event = std::make_unique<KeyEvent>(native_event);
      break;

    case ET_MOUSE_PRESSED:
    case ET_MOUSE_DRAGGED:
    case ET_MOUSE_RELEASED:
    case ET_MOUSE_MOVED:
    case ET_MOUSE_ENTERED:
    case ET_MOUSE_EXITED:
      event = std::make_unique<MouseEvent>(native_event);
      break;

    case ET_MOUSEWHEEL:
      event = std::make_unique<MouseWheelEvent>(native_event);
      break;

    case ET_SCROLL_FLING_START:
    case ET_SCROLL_FLING_CANCEL:
    case ET_SCROLL:
      event = std::make_unique<ScrollEvent>(native_event);
      break;

    case ET_TOUCH_RELEASED:
    case ET_TOUCH_PRESSED:
    case ET_TOUCH_MOVED:
    case ET_TOUCH_CANCELLED:
      event = std::make_unique<TouchEvent>(native_event);
      break;

    default:
      break;
  }
  return event;
}

int RegisterCustomEventType() {
  return ++g_custom_event_types;
}

bool ShouldDefaultToNaturalScroll() {
  return GetInternalDisplayTouchSupport() ==
         display::Display::TouchSupport::AVAILABLE;
}

display::Display::TouchSupport GetInternalDisplayTouchSupport() {
  display::Screen* screen = display::Screen::GetScreen();
  // No screen in some unit tests.
  if (!screen)
    return display::Display::TouchSupport::UNKNOWN;
  const std::vector<display::Display>& displays = screen->GetAllDisplays();
  for (auto it = displays.begin(); it != displays.end(); ++it) {
    if (it->IsInternal())
      return it->touch_support();
  }
  return display::Display::TouchSupport::UNAVAILABLE;
}

#if defined(OS_WIN)
void SetEventLatencyTickClockForTesting(const base::TickClock* clock) {
  g_tick_count_clock = clock;
}

void ComputeEventLatencyOSWinFromTickCount(ui::EventType event_type,
                                           DWORD event_time,
                                           base::TimeTicks current_time) {
  static const base::NoDestructor<GetTickCountClock> default_tick_count_clock;
  if (!g_tick_count_clock)
    g_tick_count_clock = default_tick_count_clock.get();

  if (event_type != ET_KEY_PRESSED && event_type != ET_MOUSE_PRESSED &&
      event_type != ET_TOUCH_PRESSED) {
    // Only log this metric for events that indicate strong user intent (eg.
    // clicks and keypresses), not every move event. This will detect jank that
    // causes the most user pain.
    return;
  }

  base::TimeTicks current_tick_count = g_tick_count_clock->NowTicks();
  base::TimeTicks time_stamp =
      base::TimeTicks() + base::TimeDelta::FromMilliseconds(event_time);
  // Check if the 32-bit tick count wrapped around after the event.
  if (current_tick_count < time_stamp) {
    // ::GetTickCount returns an unsigned 32-bit value, which will fit into the
    // signed 64-bit base::TimeTicks.
    current_tick_count +=
        base::TimeDelta::FromMilliseconds(std::numeric_limits<DWORD>::max());
  }

  // |event_time| is from the GetTickCount clock, which has a different 0-point
  // from |current_time| (which uses either the high-resolution timer or
  // timeGetTime). Adjust it to be compatible.
  //
  // This offset will vary by up to ~16 msec because it depends on when exactly
  // we sample the high-resolution clock. It would be more consistent to
  // calculate one offset at the start of the program and apply it every time,
  // but that consistency isn't needed for jank investigations and then we
  // would have to adjust for clock drift.
  const base::TimeDelta time_source_offset = current_time - current_tick_count;
  time_stamp += time_source_offset;

  ComputeEventLatencyOSWin(event_type, time_stamp, current_time);
}

void ComputeEventLatencyOSWinFromPerformanceCounter(
    ui::EventType event_type,
    UINT64 event_time,
    base::TimeTicks current_time) {
  if (event_type != ET_TOUCH_PRESSED) {
    // Only log this metric for events that indicate strong user intent (ie.
    // touch press), not every touch move event. This will detect jank that
    // causes the most user pain.
    return;
  }
  if (!base::TimeTicks::IsHighResolution()) {
    // The tick clock will be incompatible with |event_time|.
    return;
  }
  ComputeEventLatencyOSWin(
      event_type, base::TimeTicks::FromQPCValue(event_time), current_time);
}
#endif

void ComputeEventLatencyOS(const PlatformEvent& native_event) {
  base::TimeTicks current_time = EventTimeForNow();
  base::TimeTicks time_stamp = EventTimeFromNative(native_event);
  base::TimeDelta delta = current_time - time_stamp;

  EventType type = EventTypeFromNative(native_event);
#if defined(OS_WIN)
  // Also record windows-only metrics.
  //
  // On Windows EventTimeFromNative returns the current time instead of the
  // timestamp from |native_event|, which makes it useless for latency metrics,
  // so retrieve the time stamp directly.
  ComputeEventLatencyOSWinFromTickCount(type, native_event.time, current_time);
#endif

  switch (type) {
#if defined(OS_APPLE)
    // On Mac, ET_SCROLL and ET_MOUSEWHEEL represent the same class of events.
    case ET_SCROLL:
#endif
    case ET_MOUSEWHEEL:
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "Event.Latency.OS.MOUSE_WHEEL",
          base::saturated_cast<int>(delta.InMicroseconds()), 1, 1000000, 50);
      return;
    case ET_TOUCH_MOVED:
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "Event.Latency.OS.TOUCH_MOVED",
          base::saturated_cast<int>(delta.InMicroseconds()), 1, 1000000, 50);
      return;
    case ET_TOUCH_PRESSED:
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "Event.Latency.OS.TOUCH_PRESSED",
          base::saturated_cast<int>(delta.InMicroseconds()), 1, 1000000, 50);
      return;
    case ET_TOUCH_RELEASED:
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "Event.Latency.OS.TOUCH_RELEASED",
          base::saturated_cast<int>(delta.InMicroseconds()), 1, 1000000, 50);
      return;
    case ET_TOUCH_CANCELLED:
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "Event.Latency.OS.TOUCH_CANCELLED",
          base::saturated_cast<int>(delta.InMicroseconds()), 1, 1000000, 50);
      return;
    case ET_KEY_PRESSED:
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "Event.Latency.OS.KEY_PRESSED",
          base::saturated_cast<int>(delta.InMicroseconds()), 1, 1000000, 50);
      return;
    case ET_MOUSE_PRESSED:
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "Event.Latency.OS.MOUSE_PRESSED",
          base::saturated_cast<int>(delta.InMicroseconds()), 1, 1000000, 50);
      return;
    default:
      return;
  }
}

void ConvertEventLocationToTargetWindowLocation(
    const gfx::Point& target_window_origin,
    const gfx::Point& current_window_origin,
    ui::LocatedEvent* located_event) {
  if (current_window_origin == target_window_origin)
    return;

  DCHECK(located_event);
  gfx::Vector2d offset = current_window_origin - target_window_origin;
  gfx::PointF location_in_pixel_in_host =
      located_event->location_f() + gfx::Vector2dF(offset);
  located_event->set_location_f(location_in_pixel_in_host);
}

base::StringPiece EventTypeName(EventType type) {
  if (type >= ET_LAST)
    return "";

#define CASE_TYPE(t) \
  case t:            \
    return #t

  switch (type) {
    CASE_TYPE(ET_UNKNOWN);
    CASE_TYPE(ET_MOUSE_PRESSED);
    CASE_TYPE(ET_MOUSE_DRAGGED);
    CASE_TYPE(ET_MOUSE_RELEASED);
    CASE_TYPE(ET_MOUSE_MOVED);
    CASE_TYPE(ET_MOUSE_ENTERED);
    CASE_TYPE(ET_MOUSE_EXITED);
    CASE_TYPE(ET_KEY_PRESSED);
    CASE_TYPE(ET_KEY_RELEASED);
    CASE_TYPE(ET_MOUSEWHEEL);
    CASE_TYPE(ET_MOUSE_CAPTURE_CHANGED);
    CASE_TYPE(ET_TOUCH_RELEASED);
    CASE_TYPE(ET_TOUCH_PRESSED);
    CASE_TYPE(ET_TOUCH_MOVED);
    CASE_TYPE(ET_TOUCH_CANCELLED);
    CASE_TYPE(ET_DROP_TARGET_EVENT);
    CASE_TYPE(ET_GESTURE_SCROLL_BEGIN);
    CASE_TYPE(ET_GESTURE_SCROLL_END);
    CASE_TYPE(ET_GESTURE_SCROLL_UPDATE);
    CASE_TYPE(ET_GESTURE_SHOW_PRESS);
    CASE_TYPE(ET_GESTURE_TAP);
    CASE_TYPE(ET_GESTURE_TAP_DOWN);
    CASE_TYPE(ET_GESTURE_TAP_CANCEL);
    CASE_TYPE(ET_GESTURE_BEGIN);
    CASE_TYPE(ET_GESTURE_END);
    CASE_TYPE(ET_GESTURE_TWO_FINGER_TAP);
    CASE_TYPE(ET_GESTURE_PINCH_BEGIN);
    CASE_TYPE(ET_GESTURE_PINCH_END);
    CASE_TYPE(ET_GESTURE_PINCH_UPDATE);
    CASE_TYPE(ET_GESTURE_LONG_PRESS);
    CASE_TYPE(ET_GESTURE_LONG_TAP);
    CASE_TYPE(ET_GESTURE_SWIPE);
    CASE_TYPE(ET_GESTURE_TAP_UNCONFIRMED);
    CASE_TYPE(ET_GESTURE_DOUBLE_TAP);
    CASE_TYPE(ET_SCROLL);
    CASE_TYPE(ET_SCROLL_FLING_START);
    CASE_TYPE(ET_SCROLL_FLING_CANCEL);
    CASE_TYPE(ET_CANCEL_MODE);
    CASE_TYPE(ET_UMA_DATA);
    case ET_LAST:
      NOTREACHED();
      return "";
      // Don't include default, so that we get an error when new type is added.
  }
#undef CASE_TYPE

  NOTREACHED();
  return "";
}

std::vector<base::StringPiece> EventFlagsNames(int event_flags) {
  std::vector<base::StringPiece> names;
  names.reserve(5);  // Seems like a good starting point.
  if (!event_flags) {
    names.push_back("NONE");
    return names;
  }

  if (event_flags & EF_IS_SYNTHESIZED)
    names.push_back("IS_SYNTHESIZED");
  if (event_flags & EF_SHIFT_DOWN)
    names.push_back("SHIFT_DOWN");
  if (event_flags & EF_CONTROL_DOWN)
    names.push_back("CONTROL_DOWN");
  if (event_flags & EF_ALT_DOWN)
    names.push_back("ALT_DOWN");
  if (event_flags & EF_COMMAND_DOWN)
    names.push_back("COMMAND_DOWN");
  if (event_flags & EF_ALTGR_DOWN)
    names.push_back("ALTGR_DOWN");
  if (event_flags & EF_MOD3_DOWN)
    names.push_back("MOD3_DOWN");
  if (event_flags & EF_NUM_LOCK_ON)
    names.push_back("NUM_LOCK_ON");
  if (event_flags & EF_CAPS_LOCK_ON)
    names.push_back("CAPS_LOCK_ON");
  if (event_flags & EF_SCROLL_LOCK_ON)
    names.push_back("SCROLL_LOCK_ON");
  if (event_flags & EF_LEFT_MOUSE_BUTTON)
    names.push_back("LEFT_MOUSE_BUTTON");
  if (event_flags & EF_MIDDLE_MOUSE_BUTTON)
    names.push_back("MIDDLE_MOUSE_BUTTON");
  if (event_flags & EF_RIGHT_MOUSE_BUTTON)
    names.push_back("RIGHT_MOUSE_BUTTON");
  if (event_flags & EF_BACK_MOUSE_BUTTON)
    names.push_back("BACK_MOUSE_BUTTON");
  if (event_flags & EF_FORWARD_MOUSE_BUTTON)
    names.push_back("FORWARD_MOUSE_BUTTON");

  return names;
}

std::vector<base::StringPiece> KeyEventFlagsNames(int event_flags) {
  std::vector<base::StringPiece> names = EventFlagsNames(event_flags);
  if (!event_flags)
    return names;

  if (event_flags & EF_IME_FABRICATED_KEY)
    names.push_back("IME_FABRICATED_KEY");
  if (event_flags & EF_IS_REPEAT)
    names.push_back("IS_REPEAT");
  if (event_flags & EF_FINAL)
    names.push_back("FINAL");
  if (event_flags & EF_IS_EXTENDED_KEY)
    names.push_back("IS_EXTENDED_KEY");
  if (event_flags & EF_IS_STYLUS_BUTTON)
    names.push_back("IS_STYLUS_BUTTON");

  return names;
}

std::vector<base::StringPiece> MouseEventFlagsNames(int event_flags) {
  std::vector<base::StringPiece> names = EventFlagsNames(event_flags);
  if (!event_flags)
    return names;

  if (event_flags & EF_IS_DOUBLE_CLICK)
    names.push_back("IS_DOUBLE_CLICK");
  if (event_flags & EF_IS_TRIPLE_CLICK)
    names.push_back("IS_TRIPLE_CLICK");
  if (event_flags & EF_IS_NON_CLIENT)
    names.push_back("IS_NON_CLIENT");
  if (event_flags & EF_FROM_TOUCH)
    names.push_back("FROM_TOUCH");
  if (event_flags & EF_TOUCH_ACCESSIBILITY)
    names.push_back("TOUCH_ACCESSIBILITY");
  if (event_flags & EF_CURSOR_HIDE)
    names.push_back("CURSOR_HIDE");
  if (event_flags & EF_PRECISION_SCROLLING_DELTA)
    names.push_back("PRECISION_SCROLLING_DELTA");
  if (event_flags & EF_SCROLL_BY_PAGE)
    names.push_back("SCROLL_BY_PAGE");
  if (event_flags & EF_UNADJUSTED_MOUSE)
    names.push_back("UNADJUSTED_MOUSE");

  return names;
}

}  // namespace ui
