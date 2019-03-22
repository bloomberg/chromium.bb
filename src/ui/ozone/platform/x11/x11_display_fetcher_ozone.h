// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_X11_X11_DISPLAY_FETCHER_OZONE_H_
#define UI_OZONE_PLATFORM_X11_X11_DISPLAY_FETCHER_OZONE_H_

#include <stdint.h>

#include "ui/display/types/native_display_delegate.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/gfx/x/x11_types.h"

namespace display {
class Display;
}  // namespace display

namespace ui {

// X11DisplayFetcherOzone talks to xrandr to get the information of the outputs
// for a screen and updates Display to X11DisplayFetcherOzone::Delegate. The
// minimum required version of xrandr is 1.3.
class X11DisplayFetcherOzone : public ui::PlatformEventDispatcher {
 public:
  class Delegate {
   public:
    virtual void AddDisplay(const display::Display& display,
                            bool is_primary) = 0;
    virtual void RemoveDisplay(const display::Display& display) = 0;
  };

  explicit X11DisplayFetcherOzone(X11DisplayFetcherOzone::Delegate* delegate);
  ~X11DisplayFetcherOzone() override;

  // ui::PlatformEventDispatcher:
  bool CanDispatchEvent(const ui::PlatformEvent& event) override;
  uint32_t DispatchEvent(const ui::PlatformEvent& event) override;

 private:
  int64_t primary_display_index_ = 0;

  XDisplay* const xdisplay_;
  XID x_root_window_;

  // XRandR version. MAJOR * 100 + MINOR. Zero if no xrandr is present.
  const int xrandr_version_;

  // The base of the event numbers used to represent XRandr events used in
  // decoding events regarding output add/remove.
  int xrandr_event_base_ = 0;

  Delegate* const delegate_;

  DISALLOW_COPY_AND_ASSIGN(X11DisplayFetcherOzone);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_X11_X11_DISPLAY_FETCHER_OZONE_H_
