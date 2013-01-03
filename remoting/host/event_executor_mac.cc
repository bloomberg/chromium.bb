// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/event_executor.h"

#include <ApplicationServices/ApplicationServices.h>
#include <Carbon/Carbon.h>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "remoting/host/clipboard.h"
#include "remoting/proto/internal.pb.h"
#include "remoting/protocol/message_decoder.h"
#include "third_party/skia/include/core/SkPoint.h"
#include "third_party/skia/include/core/SkRect.h"

namespace remoting {

namespace {

using protocol::ClipboardEvent;
using protocol::KeyEvent;
using protocol::MouseEvent;

// USB to Mac keycode mapping table.
#define USB_KEYMAP(usb, xkb, win, mac) {usb, mac}
#include "ui/base/keycodes/usb_keycode_map.h"
#undef USB_KEYMAP

// skia/ext/skia_utils_mac.h only defines CGRectToSkRect().
SkIRect CGRectToSkIRect(const CGRect& rect) {
  SkIRect sk_rect = {
    SkScalarRound(rect.origin.x),
    SkScalarRound(rect.origin.y),
    SkScalarRound(rect.origin.x + rect.size.width),
    SkScalarRound(rect.origin.y + rect.size.height)
  };
  return sk_rect;
}

// A class to generate events on Mac.
class EventExecutorMac : public EventExecutor {
 public:
  explicit EventExecutorMac(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  virtual ~EventExecutorMac();

  // ClipboardStub interface.
  virtual void InjectClipboardEvent(const ClipboardEvent& event) OVERRIDE;

  // InputStub interface.
  virtual void InjectKeyEvent(const KeyEvent& event) OVERRIDE;
  virtual void InjectMouseEvent(const MouseEvent& event) OVERRIDE;

  // EventExecutor interface.
  virtual void Start(
      scoped_ptr<protocol::ClipboardStub> client_clipboard) OVERRIDE;

 private:
  // The actual implementation resides in EventExecutorWin::Core class.
  class Core : public base::RefCountedThreadSafe<Core>, public EventExecutor {
   public:
    explicit Core(scoped_refptr<base::SingleThreadTaskRunner> task_runner);

    // ClipboardStub interface.
    virtual void InjectClipboardEvent(const ClipboardEvent& event) OVERRIDE;

    // InputStub interface.
    virtual void InjectKeyEvent(const KeyEvent& event) OVERRIDE;
    virtual void InjectMouseEvent(const MouseEvent& event) OVERRIDE;

    // EventExecutor interface.
    virtual void Start(
        scoped_ptr<protocol::ClipboardStub> client_clipboard) OVERRIDE;

    void Stop();

   private:
    friend class base::RefCountedThreadSafe<Core>;
    virtual ~Core();

    scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
    SkIPoint mouse_pos_;
    uint32 mouse_button_state_;
    scoped_ptr<Clipboard> clipboard_;

    DISALLOW_COPY_AND_ASSIGN(Core);
  };

  scoped_refptr<Core> core_;

  DISALLOW_COPY_AND_ASSIGN(EventExecutorMac);
};

EventExecutorMac::EventExecutorMac(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  core_ = new Core(task_runner);
}

EventExecutorMac::~EventExecutorMac() {
  core_->Stop();
}

void EventExecutorMac::InjectClipboardEvent(const ClipboardEvent& event) {
  core_->InjectClipboardEvent(event);
}

void EventExecutorMac::InjectKeyEvent(const KeyEvent& event) {
  core_->InjectKeyEvent(event);
}

void EventExecutorMac::InjectMouseEvent(const MouseEvent& event) {
  core_->InjectMouseEvent(event);
}

void EventExecutorMac::Start(
    scoped_ptr<protocol::ClipboardStub> client_clipboard) {
  core_->Start(client_clipboard.Pass());
}

EventExecutorMac::Core::Core(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : task_runner_(task_runner),
      mouse_button_state_(0),
      clipboard_(Clipboard::Create()) {
  // Ensure that local hardware events are not suppressed after injecting
  // input events.  This allows LocalInputMonitor to detect if the local mouse
  // is being moved whilst a remote user is connected.
  // This API is deprecated, but it is needed when using the deprecated
  // injection APIs.
  // If the non-deprecated injection APIs were used instead, the equivalent of
  // this line would not be needed, as OS X defaults to _not_ suppressing local
  // inputs in that case.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  CGSetLocalEventsSuppressionInterval(0.0);
#pragma clang diagnostic pop
}

void EventExecutorMac::Core::InjectClipboardEvent(const ClipboardEvent& event) {
  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(
        FROM_HERE, base::Bind(&Core::InjectClipboardEvent, this, event));
    return;
  }

  clipboard_->InjectClipboardEvent(event);
}

void EventExecutorMac::Core::InjectKeyEvent(const KeyEvent& event) {
  // HostEventDispatcher should filter events missing the pressed field.
  DCHECK(event.has_pressed());
  DCHECK(event.has_usb_keycode());

  int keycode = UsbKeycodeToNativeKeycode(event.usb_keycode());

  VLOG(3) << "Converting USB keycode: " << std::hex << event.usb_keycode()
          << " to keycode: " << keycode << std::dec;

  // If we couldn't determine the Mac virtual key code then ignore the event.
  if (keycode == kInvalidKeycode)
    return;

  // We use the deprecated event injection API because the new one doesn't
  // work with switched-out sessions (curtain mode).
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  CGError error = CGPostKeyboardEvent(0, keycode, event.pressed());
#pragma clang diagnostic pop
  if (error != kCGErrorSuccess) {
    LOG(WARNING) << "CGPostKeyboardEvent error " << error;
  }
}

void EventExecutorMac::Core::InjectMouseEvent(const MouseEvent& event) {
  if (event.has_x() && event.has_y()) {
    // On multi-monitor systems (0,0) refers to the top-left of the "main"
    // display, whereas our coordinate scheme places (0,0) at the top-left of
    // the bounding rectangle around all the displays, so we need to translate
    // accordingly.
    // TODO(wez): Move display config tracking into a separate class used both
    // here and in the Capturer.

    // Set the mouse position assuming single-monitor.
    mouse_pos_ = SkIPoint::Make(event.x(), event.y());

    // Determine how many active displays there are.
    CGDisplayCount display_count;
    CGError error = CGGetActiveDisplayList(0, NULL, &display_count);
    CHECK_EQ(error, CGDisplayNoErr);

    if (display_count > 1) {
      // Determine the bounding box of the displays, to get the top-left origin.
      std::vector<CGDirectDisplayID> display_ids(display_count);
      error = CGGetActiveDisplayList(display_count, &display_ids[0],
                                     &display_count);
      CHECK_EQ(error, CGDisplayNoErr);
      CHECK_EQ(display_count, display_ids.size());

      SkIRect desktop_bounds = SkIRect::MakeEmpty();
      for (unsigned int d = 0; d < display_count; ++d) {
        CGRect display_bounds = CGDisplayBounds(display_ids[d]);
        desktop_bounds.join(CGRectToSkIRect(display_bounds));
      }

      // Adjust the injected mouse event position.
      mouse_pos_ += SkIPoint::Make(desktop_bounds.left(), desktop_bounds.top());
    }

    VLOG(3) << "Moving mouse to " << event.x() << "," << event.y();
  }
  if (event.has_button() && event.has_button_down()) {
    if (event.button() >= 1 && event.button() <= 3) {
      VLOG(2) << "Button " << event.button()
              << (event.button_down() ? " down" : " up");
      int button_change = 1 << (event.button() - 1);
      if (event.button_down())
        mouse_button_state_ |= button_change;
      else
        mouse_button_state_ &= ~button_change;
    } else {
      VLOG(1) << "Unknown mouse button: " << event.button();
    }
  }
  // We use the deprecated CGPostMouseEvent API because we receive low-level
  // mouse events, whereas CGEventCreateMouseEvent is for injecting higher-level
  // events. For example, the deprecated APIs will detect double-clicks or drags
  // in a way that is consistent with how they would be generated using a local
  // mouse, whereas the new APIs expect us to inject these higher-level events
  // directly.
  CGPoint position = CGPointMake(mouse_pos_.x(), mouse_pos_.y());
  enum {
    LeftBit = 1 << (MouseEvent::BUTTON_LEFT - 1),
    MiddleBit = 1 << (MouseEvent::BUTTON_MIDDLE - 1),
    RightBit = 1 << (MouseEvent::BUTTON_RIGHT - 1)
  };
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  CGError error = CGPostMouseEvent(position, true, 3,
                                   (mouse_button_state_ & LeftBit) != 0,
                                   (mouse_button_state_ & RightBit) != 0,
                                   (mouse_button_state_ & MiddleBit) != 0);
#pragma clang diagnostic pop
  if (error != kCGErrorSuccess) {
    LOG(WARNING) << "CGPostMouseEvent error " << error;
  }

  if (event.has_wheel_delta_x() && event.has_wheel_delta_y()) {
    int delta_x = static_cast<int>(event.wheel_delta_x());
    int delta_y = static_cast<int>(event.wheel_delta_y());
    base::mac::ScopedCFTypeRef<CGEventRef> event(
        CGEventCreateScrollWheelEvent(
            NULL, kCGScrollEventUnitPixel, 2, delta_y, delta_x));
    if (event)
      CGEventPost(kCGHIDEventTap, event);
  }
}

void EventExecutorMac::Core::Start(
    scoped_ptr<protocol::ClipboardStub> client_clipboard) {
  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&Core::Start, this, base::Passed(&client_clipboard)));
    return;
  }

  clipboard_->Start(client_clipboard.Pass());
}

void EventExecutorMac::Core::Stop() {
  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(FROM_HERE, base::Bind(&Core::Stop, this));
    return;
  }

  clipboard_->Stop();
}

EventExecutorMac::Core::~Core() {
}

}  // namespace

scoped_ptr<EventExecutor> EventExecutor::Create(
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner) {
  return scoped_ptr<EventExecutor>(new EventExecutorMac(main_task_runner));
}

}  // namespace remoting
