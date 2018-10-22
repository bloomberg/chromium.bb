// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Needed for defined(OS_WIN)
#include "build/build_config.h"

// Windows headers must come first.
#if defined(OS_WIN)
#include <windows.h>

#include <timeapi.h>
#endif

// Proceed with header includes in usual order.
#include "content/browser/scheduler/responsiveness/native_event_observer.h"

#include "ui/events/platform/platform_event_source.h"

#if defined(USE_X11)
#include "ui/events/platform/x11/x11_event_source.h"  // nogncheck
#elif defined(USE_OZONE)
#include "ui/events/event.h"
#endif

#if defined(OS_LINUX)
#include "ui/events/platform_event.h"
#endif

#if defined(OS_WIN)
#include "base/message_loop/message_loop_current.h"
#endif

namespace content {
namespace responsiveness {

#if defined(OS_WIN) || (defined(OS_LINUX) && defined(USE_X11))

namespace {

// Clamps a TimeDelta to be within [0 seconds, 30 seconds].
// Relies on the assumption that |delta| is >= 0 seconds.
base::TimeDelta ClampDeltaFromExternalSource(const base::TimeDelta& delta) {
  DCHECK_GE(delta, base::TimeDelta());

  // Ignore pathologically long deltas. External source is probably having
  // issues.
  constexpr base::TimeDelta pathologically_long_duration =
      base::TimeDelta::FromSeconds(30);
  if (delta > pathologically_long_duration)
    return base::TimeDelta();

  return delta;
}

}  // namespace

#endif  // defined(OS_WIN) || (defined(OS_LINUX) && defined(USE_X11))

NativeEventObserver::NativeEventObserver(
    WillRunEventCallback will_run_event_callback,
    DidRunEventCallback did_run_event_callback)
    : will_run_event_callback_(will_run_event_callback),
      did_run_event_callback_(did_run_event_callback) {
  RegisterObserver();
}

NativeEventObserver::~NativeEventObserver() {
  DeregisterObserver();
}

#if defined(OS_LINUX)
void NativeEventObserver::RegisterObserver() {
  ui::PlatformEventSource::GetInstance()->AddPlatformEventObserver(this);
}
void NativeEventObserver::DeregisterObserver() {
  ui::PlatformEventSource::GetInstance()->RemovePlatformEventObserver(this);
}

void NativeEventObserver::WillProcessEvent(const ui::PlatformEvent& event) {
  will_run_event_callback_.Run(&event);
}

void NativeEventObserver::DidProcessEvent(const ui::PlatformEvent& event) {
#if defined(USE_OZONE)
  did_run_event_callback_.Run(&event, event->time_stamp());
#elif defined(USE_X11)
  // X11 uses a uint32_t on the wire protocol. Xlib casts this to an unsigned
  // long by prepending with 0s. We cast back to a uint32_t so that subtraction
  // works properly when the timestamp overflows back to 0.
  uint32_t event_server_time_ms =
      static_cast<uint32_t>(ui::X11EventSource::GetInstance()->GetTimestamp());
  uint32_t current_server_time_ms = static_cast<uint32_t>(
      ui::X11EventSource::GetInstance()->GetCurrentServerTime());

  // On X11, event times are in X11 Server time. To convert to base::TimeTicks,
  // we perform a round-trip to the X11 Server, subtract the two times to get a
  // TimeDelta, and then subtract that from base::TimeTicks::Now(). Since we're
  // working with units of time from an external source, we clamp the TimeDelta
  // to reasonable values.
  uint32_t delta_ms = current_server_time_ms - event_server_time_ms;
  base::TimeDelta delta = base::TimeDelta::FromMilliseconds(delta_ms);
  base::TimeDelta sanitized = ClampDeltaFromExternalSource(delta);

  did_run_event_callback_.Run(&event, base::TimeTicks::Now() - sanitized);
#else
#error
#endif
}
#endif  // defined(OS_LINUX)

#if defined(OS_WIN)
void NativeEventObserver::RegisterObserver() {
  base::MessageLoopCurrentForUI::Get()->AddMessagePumpObserver(this);
}
void NativeEventObserver::DeregisterObserver() {
  base::MessageLoopCurrentForUI::Get()->RemoveMessagePumpObserver(this);
}
void NativeEventObserver::WillDispatchMSG(const MSG& msg) {
  will_run_event_callback_.Run(&msg);
}
void NativeEventObserver::DidDispatchMSG(const MSG& msg) {
  // On Windows, MSG.time is in units of milliseconds since system started. It
  // uses the timer exposed by ::GetTickCount().
  // https://blogs.msdn.microsoft.com/oldnewthing/20140122-00/?p=2013
  //
  // This timer has ~16ms granularity. This is okay for us since we require
  // ~100ms granularity.
  // https://randomascii.wordpress.com/2013/05/09/timegettime-versus-gettickcount/
  //
  // To convert MSG.time to TimeTicks, we subtract MSG.time from
  // ::GetTickCount() to create a TimeDelta, and then subtract that from
  // TimeTicks::Now().
  //
  // We cast both values to DWORD [uint32], perform subtraction to get a uint32,
  // and then shove that into a TimeDelta.
  //
  // Note: Both measurements of time can experience rollover. If one of them has
  // rolled over but the other has not, then the result will be very large.
  // ClampDeltaFromExternalSource takes care of this, in addition to any other
  // odd timing issues that may come up [e.g. MSG.time time-traveling to give a
  // larger value than ::timeGetTime() -- this has not been observed in practice
  // but since we don't control the code in question, we trust nothing].
  base::TimeDelta delta = base::TimeDelta::FromMilliseconds(
      ::GetTickCount() - static_cast<DWORD>(msg.time));
  base::TimeDelta sanitized = ClampDeltaFromExternalSource(delta);
  did_run_event_callback_.Run(&msg, base::TimeTicks::Now() - sanitized);
}
#endif  // defined(OS_WIN)

#if defined(OS_ANDROID) || defined(OS_FUCHSIA)
void NativeEventObserver::RegisterObserver() {}
void NativeEventObserver::DeregisterObserver() {}
#endif  // defined(OS_ANDROID) || defined(OS_FUCHSIA)

}  // namespace responsiveness
}  // namespace content
