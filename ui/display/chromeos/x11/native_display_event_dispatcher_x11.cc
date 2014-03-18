// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/chromeos/x11/native_display_event_dispatcher_x11.h"
#include "ui/display/chromeos/x11/display_mode_x11.h"
#include "ui/display/chromeos/x11/display_snapshot_x11.h"

#include <X11/extensions/Xrandr.h>

namespace ui {

NativeDisplayEventDispatcherX11::NativeDisplayEventDispatcherX11(
    NativeDisplayDelegateX11::HelperDelegate* delegate,
    int xrandr_event_base)
    : delegate_(delegate), xrandr_event_base_(xrandr_event_base) {}

NativeDisplayEventDispatcherX11::~NativeDisplayEventDispatcherX11() {}

uint32_t NativeDisplayEventDispatcherX11::Dispatch(
    const base::NativeEvent& event) {
  if (event->type - xrandr_event_base_ == RRScreenChangeNotify) {
    VLOG(1) << "Received RRScreenChangeNotify event";
    delegate_->UpdateXRandRConfiguration(event);
    return POST_DISPATCH_PERFORM_DEFAULT;
  }

  // Bail out early for everything except RRNotify_OutputChange events
  // about an output getting connected or disconnected.
  if (event->type - xrandr_event_base_ != RRNotify)
    return POST_DISPATCH_PERFORM_DEFAULT;
  const XRRNotifyEvent* notify_event = reinterpret_cast<XRRNotifyEvent*>(event);
  if (notify_event->subtype != RRNotify_OutputChange)
    return POST_DISPATCH_PERFORM_DEFAULT;
  const XRROutputChangeNotifyEvent* output_change_event =
      reinterpret_cast<XRROutputChangeNotifyEvent*>(event);
  const int action = output_change_event->connection;
  if (action != RR_Connected && action != RR_Disconnected)
    return POST_DISPATCH_PERFORM_DEFAULT;

  const bool connected = (action == RR_Connected);
  VLOG(1) << "Received RRNotify_OutputChange event:"
          << " output=" << output_change_event->output
          << " crtc=" << output_change_event->crtc
          << " mode=" << output_change_event->mode
          << " action=" << (connected ? "connected" : "disconnected");

  bool found_changed_output = false;
  const std::vector<DisplaySnapshot*>& cached_outputs =
      delegate_->GetCachedOutputs();
  for (std::vector<DisplaySnapshot*>::const_iterator it =
           cached_outputs.begin();
       it != cached_outputs.end();
       ++it) {
    const DisplaySnapshotX11* x11_output =
        static_cast<const DisplaySnapshotX11*>(*it);
    const DisplayModeX11* x11_mode =
        static_cast<const DisplayModeX11*>(x11_output->current_mode());

    if (x11_output->output() == output_change_event->output) {
      if (connected && x11_output->crtc() == output_change_event->crtc &&
          x11_mode->mode_id() == output_change_event->mode) {
        VLOG(1) << "Ignoring event describing already-cached state";
        return POST_DISPATCH_PERFORM_DEFAULT;
      }
      found_changed_output = true;
      break;
    }
  }

  if (!connected && !found_changed_output) {
    VLOG(1) << "Ignoring event describing already-disconnected output";
    return POST_DISPATCH_PERFORM_DEFAULT;
  }

  delegate_->NotifyDisplayObservers();

  return POST_DISPATCH_PERFORM_DEFAULT;
}

}  // namespace ui
