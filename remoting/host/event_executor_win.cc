// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/event_executor.h"

#include <windows.h>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "remoting/base/util.h"
#include "remoting/host/clipboard.h"
#include "remoting/proto/event.pb.h"
// SkSize.h assumes that stdint.h-style types are already defined.
#include "third_party/skia/include/core/SkTypes.h"
#include "third_party/skia/include/core/SkSize.h"

namespace remoting {

namespace {

using protocol::ClipboardEvent;
using protocol::KeyEvent;
using protocol::MouseEvent;

// USB to XKB keycode map table.
#define USB_KEYMAP(usb, xkb, win, mac) {usb, win}
#include "ui/base/keycodes/usb_keycode_map.h"
#undef USB_KEYMAP

// A class to generate events on Windows.
class EventExecutorWin : public EventExecutor {
 public:
  EventExecutorWin(scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
                   scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner);
  virtual ~EventExecutorWin();

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
  class Core : public base::RefCountedThreadSafe<Core> {
   public:
    Core(scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
         scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner);

    // Mirrors the ClipboardStub interface.
    void InjectClipboardEvent(const ClipboardEvent& event);

    // Mirrors the InputStub interface.
    void InjectKeyEvent(const KeyEvent& event);
    void InjectMouseEvent(const MouseEvent& event);

    // Mirrors the EventExecutor interface.
    void Start(scoped_ptr<protocol::ClipboardStub> client_clipboard);

    void Stop();

   private:
    friend class base::RefCountedThreadSafe<Core>;
    virtual ~Core();

    void HandleKey(const KeyEvent& event);
    void HandleMouse(const MouseEvent& event);

    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;
    scoped_ptr<Clipboard> clipboard_;

    DISALLOW_COPY_AND_ASSIGN(Core);
  };

  scoped_refptr<Core> core_;

  DISALLOW_COPY_AND_ASSIGN(EventExecutorWin);
};

EventExecutorWin::EventExecutorWin(
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner) {
  core_ = new Core(main_task_runner, ui_task_runner);
}

EventExecutorWin::~EventExecutorWin() {
  core_->Stop();
}

void EventExecutorWin::InjectClipboardEvent(const ClipboardEvent& event) {
  core_->InjectClipboardEvent(event);
}

void EventExecutorWin::InjectKeyEvent(const KeyEvent& event) {
  core_->InjectKeyEvent(event);
}

void EventExecutorWin::InjectMouseEvent(const MouseEvent& event) {
  core_->InjectMouseEvent(event);
}

void EventExecutorWin::Start(
    scoped_ptr<protocol::ClipboardStub> client_clipboard) {
  core_->Start(client_clipboard.Pass());
}

EventExecutorWin::Core::Core(
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner)
    : main_task_runner_(main_task_runner),
      ui_task_runner_(ui_task_runner),
      clipboard_(Clipboard::Create()) {
}

void EventExecutorWin::Core::InjectClipboardEvent(const ClipboardEvent& event) {
  if (!ui_task_runner_->BelongsToCurrentThread()) {
    ui_task_runner_->PostTask(
        FROM_HERE, base::Bind(&Core::InjectClipboardEvent, this, event));
    return;
  }

  // |clipboard_| will ignore unknown MIME-types, and verify the data's format.
  clipboard_->InjectClipboardEvent(event);
}

void EventExecutorWin::Core::InjectKeyEvent(const KeyEvent& event) {
  if (!main_task_runner_->BelongsToCurrentThread()) {
    main_task_runner_->PostTask(FROM_HERE,
                                base::Bind(&Core::InjectKeyEvent, this, event));
    return;
  }

  HandleKey(event);
}

void EventExecutorWin::Core::InjectMouseEvent(const MouseEvent& event) {
  if (!main_task_runner_->BelongsToCurrentThread()) {
    main_task_runner_->PostTask(
        FROM_HERE, base::Bind(&Core::InjectMouseEvent, this, event));
    return;
  }

  HandleMouse(event);
}

void EventExecutorWin::Core::Start(
    scoped_ptr<protocol::ClipboardStub> client_clipboard) {
  if (!ui_task_runner_->BelongsToCurrentThread()) {
    ui_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&Core::Start, this, base::Passed(&client_clipboard)));
    return;
  }

  clipboard_->Start(client_clipboard.Pass());
}

void EventExecutorWin::Core::Stop() {
  if (!ui_task_runner_->BelongsToCurrentThread()) {
    ui_task_runner_->PostTask(FROM_HERE, base::Bind(&Core::Stop, this));
    return;
  }

  clipboard_->Stop();
}

EventExecutorWin::Core::~Core() {
}

void EventExecutorWin::Core::HandleKey(const KeyEvent& event) {
  // HostEventDispatcher should filter events missing the pressed field.
  if (!event.has_pressed() || !event.has_usb_keycode())
    return;

  // Reset the system idle suspend timeout.
  SetThreadExecutionState(ES_SYSTEM_REQUIRED);

  int scancode = UsbKeycodeToNativeKeycode(event.usb_keycode());

  VLOG(3) << "Converting USB keycode: " << std::hex << event.usb_keycode()
          << " to scancode: " << scancode << std::dec;

  // Ignore events which can't be mapped.
  if (scancode == InvalidNativeKeycode())
    return;

  // Populate the a Windows INPUT structure for the event.
  INPUT input;
  memset(&input, 0, sizeof(input));
  input.type = INPUT_KEYBOARD;
  input.ki.time = 0;
  input.ki.dwFlags = KEYEVENTF_SCANCODE;
  if (!event.pressed())
    input.ki.dwFlags |= KEYEVENTF_KEYUP;

  // Windows scancodes are only 8-bit, so store the low-order byte into the
  // event and set the extended flag if any high-order bits are set. The only
  // high-order values we should see are 0xE0 or 0xE1. The extended bit usually
  // distinguishes keys with the same meaning, e.g. left & right shift.
  input.ki.wScan = scancode & 0xFF;
  if ((scancode & 0xFF00) != 0x0000)
    input.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;

  if (SendInput(1, &input, sizeof(INPUT)) == 0)
    LOG_GETLASTERROR(ERROR) << "Failed to inject a key event";
}

void EventExecutorWin::Core::HandleMouse(const MouseEvent& event) {
  // Reset the system idle suspend timeout.
  SetThreadExecutionState(ES_SYSTEM_REQUIRED);

  // TODO(garykac) Collapse mouse (x,y) and button events into a single
  // input event when possible.
  if (event.has_x() && event.has_y()) {
    int x = event.x();
    int y = event.y();

    INPUT input;
    input.type = INPUT_MOUSE;
    input.mi.time = 0;
    SkISize screen_size(SkISize::Make(GetSystemMetrics(SM_CXVIRTUALSCREEN),
                                      GetSystemMetrics(SM_CYVIRTUALSCREEN)));
    if ((screen_size.width() > 1) && (screen_size.height() > 1)) {
      x = std::max(0, std::min(screen_size.width(), x));
      y = std::max(0, std::min(screen_size.height(), y));
      input.mi.dx = static_cast<int>((x * 65535) / (screen_size.width() - 1));
      input.mi.dy = static_cast<int>((y * 65535) / (screen_size.height() - 1));
      input.mi.dwFlags =
          MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;
      if (SendInput(1, &input, sizeof(INPUT)) == 0)
        LOG_GETLASTERROR(ERROR) << "Failed to inject a mouse move event";
    }
  }

  int wheel_delta_x = 0;
  int wheel_delta_y = 0;
  if (event.has_wheel_delta_x() && event.has_wheel_delta_y()) {
    wheel_delta_x = static_cast<int>(event.wheel_delta_x());
    wheel_delta_y = static_cast<int>(event.wheel_delta_y());
  }

  if (wheel_delta_x != 0 || wheel_delta_y != 0) {
    INPUT wheel;
    wheel.type = INPUT_MOUSE;
    wheel.mi.time = 0;

    if (wheel_delta_x != 0) {
      wheel.mi.mouseData = wheel_delta_x;
      wheel.mi.dwFlags = MOUSEEVENTF_HWHEEL;
      if (SendInput(1, &wheel, sizeof(INPUT)) == 0)
        LOG_GETLASTERROR(ERROR) << "Failed to inject a mouse wheel(x) event";
    }
    if (wheel_delta_y != 0) {
      wheel.mi.mouseData = wheel_delta_y;
      wheel.mi.dwFlags = MOUSEEVENTF_WHEEL;
      if (SendInput(1, &wheel, sizeof(INPUT)) == 0)
        LOG_GETLASTERROR(ERROR) << "Failed to inject a mouse wheel(y) event";
    }
  }

  if (event.has_button() && event.has_button_down()) {
    INPUT button_event;
    button_event.type = INPUT_MOUSE;
    button_event.mi.time = 0;
    button_event.mi.dx = 0;
    button_event.mi.dy = 0;

    MouseEvent::MouseButton button = event.button();
    bool down = event.button_down();

    // If the host is configured to swap left & right buttons, inject swapped
    // events to un-do that re-mapping.
    if (GetSystemMetrics(SM_SWAPBUTTON)) {
      if (button == MouseEvent::BUTTON_LEFT) {
        button = MouseEvent::BUTTON_RIGHT;
      } else if (button == MouseEvent::BUTTON_RIGHT) {
        button = MouseEvent::BUTTON_LEFT;
      }
    }

    if (button == MouseEvent::BUTTON_LEFT) {
      button_event.mi.dwFlags =
          down ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
    } else if (button == MouseEvent::BUTTON_MIDDLE) {
      button_event.mi.dwFlags =
          down ? MOUSEEVENTF_MIDDLEDOWN : MOUSEEVENTF_MIDDLEUP;
    } else if (button == MouseEvent::BUTTON_RIGHT) {
      button_event.mi.dwFlags =
          down ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP;
    } else {
      button_event.mi.dwFlags =
          down ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
    }

    if (SendInput(1, &button_event, sizeof(INPUT)) == 0)
      LOG_GETLASTERROR(ERROR) << "Failed to inject a mouse button event";
  }
}

}  // namespace

scoped_ptr<EventExecutor> EventExecutor::Create(
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner) {
  return scoped_ptr<EventExecutor>(
      new EventExecutorWin(main_task_runner, ui_task_runner));
}

}  // namespace remoting
