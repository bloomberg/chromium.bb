// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_CHROMEOS_X11_NATIVE_DISPLAY_EVENT_DISPATCHER_X11_H_
#define UI_DISPLAY_CHROMEOS_X11_NATIVE_DISPLAY_EVENT_DISPATCHER_X11_H_

#include "base/message_loop/message_pump_dispatcher.h"
#include "ui/display/chromeos/x11/native_display_delegate_x11.h"

namespace ui {

class DISPLAY_EXPORT NativeDisplayEventDispatcherX11
    : public base::MessagePumpDispatcher {
 public:
  NativeDisplayEventDispatcherX11(
      NativeDisplayDelegateX11::HelperDelegate* delegate,
      int xrandr_event_base);
  virtual ~NativeDisplayEventDispatcherX11();

  // base::MessagePumpDispatcher overrides:
  //
  // Called when an RRNotify event is received.  The implementation is
  // interested in the cases of RRNotify events which correspond to output
  // add/remove events.  Note that Output add/remove events are sent in response
  // to our own reconfiguration operations so spurious events are common.
  // Spurious events will have no effect.
  virtual uint32_t Dispatch(const base::NativeEvent& event) OVERRIDE;

 private:
  NativeDisplayDelegateX11::HelperDelegate* delegate_;  // Not owned.

  // The base of the event numbers used to represent XRandr events used in
  // decoding events regarding output add/remove.
  int xrandr_event_base_;

  DISALLOW_COPY_AND_ASSIGN(NativeDisplayEventDispatcherX11);
};

}  // namespace ui

#endif  // UI_DISPLAY_CHROMEOS_X11_NATIVE_DISPLAY_EVENT_DISPATCHER_X11_H_
