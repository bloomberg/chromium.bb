// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_MOUSE_MOVE_OBSERVER_H_
#define REMOTING_HOST_MOUSE_MOVE_OBSERVER_H_

struct SkIPoint;

namespace remoting {

// Interface used by LocalInputMonitor to notify about local mouse
// movement.
class MouseMoveObserver {
 public:
  virtual void OnLocalMouseMoved(const SkIPoint& new_pos) = 0;

 protected:
  virtual ~MouseMoveObserver() {}
};

}  // namespace remoting

#endif  // REMOTING_HOST_MOUSE_MOVE_OBSERVER_H_
