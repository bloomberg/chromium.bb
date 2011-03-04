// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/event_executor_mac.h"

#include <ApplicationServices/ApplicationServices.h>

#include "base/message_loop.h"
#include "base/task.h"
#include "remoting/host/capturer.h"
#include "remoting/protocol/message_decoder.h"
#include "remoting/proto/internal.pb.h"

namespace remoting {

using protocol::MouseEvent;
using protocol::KeyEvent;

EventExecutorMac::EventExecutorMac(
    MessageLoopForUI* message_loop, Capturer* capturer)
    : message_loop_(message_loop),
      capturer_(capturer), last_x_(0), last_y_(0) {
}

EventExecutorMac::~EventExecutorMac() {
}

void EventExecutorMac::InjectKeyEvent(const KeyEvent* event, Task* done) {
  done->Run();
  delete done;
}

void EventExecutorMac::InjectMouseEvent(const MouseEvent* event, Task* done) {
  CGEventType event_type = kCGEventNull;

  if (event->has_x() && event->has_y()) {
    // TODO(wez): Checking the validity of the MouseEvent should be done in core
    // cross-platform code, not here!
    // TODO(wez): This code assumes that MouseEvent(0,0) (top-left of client view)
    // corresponds to local (0,0) (top-left of primary monitor).  That won't in
    // general be true on multi-monitor systems, though.
    int width = capturer_->width_most_recent();
    int height = capturer_->height_most_recent();
    if (event->x() >= 0 || event->y() >= 0 ||
        event->x() < width || event->y() < height) {

      VLOG(3) << "Moving mouse to " << event->x() << "," << event->y();

      event_type = kCGEventMouseMoved;
      last_x_ = event->x();
      last_y_ = event->y();
    }
  }

  CGPoint position = CGPointMake(last_x_, last_y_);
  CGMouseButton mouse_button = 0;

  if (event->has_button() && event->has_button_down()) {

    VLOG(2) << "Button " << event->button()
            << (event->button_down() ? " down" : " up");

    event_type = event->button_down() ? kCGEventOtherMouseDown
                                      : kCGEventOtherMouseUp;
    if (MouseEvent::BUTTON_LEFT == event->button()) {
      event_type = event->button_down() ? kCGEventLeftMouseDown
                                        : kCGEventLeftMouseUp;
      mouse_button = kCGMouseButtonLeft; // not strictly necessary
    } else if (MouseEvent::BUTTON_RIGHT == event->button()) {
      event_type = event->button_down() ? kCGEventRightMouseDown
                                        : kCGEventRightMouseUp;
      mouse_button = kCGMouseButtonRight; // not strictly necessary
    } else if (MouseEvent::BUTTON_MIDDLE == event->button()) {
      mouse_button = kCGMouseButtonCenter;
    } else {
      VLOG(1) << "Unknown mouse button!" << event->button();
    }
  }

  if (event->has_wheel_offset_x() && event->has_wheel_offset_y()) {
    // TODO(wez): Should set event_type == kCGEventScrollWheel and
    // populate fields for a CGEventCreateScrollWheelEvent() here.
    NOTIMPLEMENTED() << "No scroll wheel support yet.";
  }

  if (event_type != kCGEventNull) {
    CGEventRef mouse_event = CGEventCreateMouseEvent(NULL, event_type,
                                                     position, mouse_button);
    CGEventPost(kCGSessionEventTap, mouse_event);
    CFRelease(mouse_event);
  }

  done->Run();
  delete done;
}

protocol::InputStub* CreateEventExecutor(MessageLoopForUI* message_loop,
                                         Capturer* capturer) {
  return new EventExecutorMac(message_loop, capturer);
}

}  // namespace remoting
