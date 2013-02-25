// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_REMOTE_INPUT_FILTER_H_
#define REMOTING_HOST_REMOTE_INPUT_FILTER_H_

#include <list>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/time.h"
#include "remoting/protocol/input_event_tracker.h"
#include "remoting/protocol/input_stub.h"
#include "third_party/skia/include/core/SkPoint.h"

namespace remoting {

// Filtering InputStub that filters remotely-injected input if it has been
// notified of local input recently.
class RemoteInputFilter : public protocol::InputStub {
 public:
  // Creates a filter forwarding events to the specified InputEventTracker.
  // The filter needs a tracker to release buttons & keys when blocking input.
  explicit RemoteInputFilter(protocol::InputEventTracker* event_tracker);
  virtual ~RemoteInputFilter();

  // Informs the filter that local mouse activity has been detected.  If the
  // activity does not match events we injected then we assume that it is local,
  // and block remote input for a short while.
  void LocalMouseMoved(const SkIPoint& mouse_pos);

  // Informs the filter that injecting input causes an echo.
  void SetExpectLocalEcho(bool expect_local_echo);

  // InputStub overrides.
  virtual void InjectKeyEvent(const protocol::KeyEvent& event) OVERRIDE;
  virtual void InjectMouseEvent(const protocol::MouseEvent& event) OVERRIDE;

 private:
  bool ShouldIgnoreInput() const;

  protocol::InputEventTracker* event_tracker_;

  // Queue of recently-injected mouse positions used to distinguish echoes of
  // injected events from movements from a local input device.
  std::list<SkIPoint> injected_mouse_positions_;

  // Time at which local input events were most recently observed.
  base::TimeTicks latest_local_input_time_;

  // If |true| than the filter assumes that injecting input causes an echo.
  bool expect_local_echo_;

  DISALLOW_COPY_AND_ASSIGN(RemoteInputFilter);
};

}  // namespace remoting

#endif  // REMOTING_HOST_REMOTE_INPUT_FILTER_H_
