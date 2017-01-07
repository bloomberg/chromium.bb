// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_MANAGER_CHROMEOS_X11_NATIVE_DISPLAY_EVENT_DISPATCHER_X11_H_
#define UI_DISPLAY_MANAGER_CHROMEOS_X11_NATIVE_DISPLAY_EVENT_DISPATCHER_X11_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "ui/display/manager/chromeos/x11/native_display_delegate_x11.h"
#include "ui/events/platform/platform_event_dispatcher.h"

namespace display {

// The implementation is interested in the cases of RRNotify events which
// correspond to output add/remove events. Note that Output add/remove events
// are sent in response to our own reconfiguration operations so spurious events
// are common. Spurious events will have no effect.
class DISPLAY_MANAGER_EXPORT NativeDisplayEventDispatcherX11
    : public ui::PlatformEventDispatcher {
 public:
  NativeDisplayEventDispatcherX11(
      NativeDisplayDelegateX11::HelperDelegate* delegate,
      int xrandr_event_base);
  ~NativeDisplayEventDispatcherX11() override;

  // ui::PlatformEventDispatcher:
  bool CanDispatchEvent(const ui::PlatformEvent& event) override;
  uint32_t DispatchEvent(const ui::PlatformEvent& event) override;

  void SetTickClockForTest(std::unique_ptr<base::TickClock> tick_clock);

  // How long the cached output is valid after startup.
  static const int kUseCacheAfterStartupMs;

 private:
  NativeDisplayDelegateX11::HelperDelegate* delegate_;  // Not owned.

  // The base of the event numbers used to represent XRandr events used in
  // decoding events regarding output add/remove.
  int xrandr_event_base_;

  base::TimeTicks startup_time_;

  std::unique_ptr<base::TickClock> tick_clock_;

  DISALLOW_COPY_AND_ASSIGN(NativeDisplayEventDispatcherX11);
};

}  // namespace display

#endif  // UI_DISPLAY_MANAGER_CHROMEOS_X11_NATIVE_DISPLAY_EVENT_DISPATCHER_X11_H_
