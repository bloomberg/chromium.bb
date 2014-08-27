// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/input_injector.h"

#include <windows.h>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "remoting/base/util.h"
#include "remoting/host/clipboard.h"
#include "remoting/proto/event.pb.h"
#include "ui/events/keycodes/dom4/keycode_converter.h"

namespace remoting {

namespace {

// Helper used to call SendInput() API.
void SendKeyboardInput(uint32_t flags, uint16_t scancode) {
  // Populate a Windows INPUT structure for the event.
  INPUT input;
  memset(&input, 0, sizeof(input));
  input.type = INPUT_KEYBOARD;
  input.ki.time = 0;
  input.ki.dwFlags = flags;
  input.ki.wScan = scancode;

  if ((flags & KEYEVENTF_UNICODE) == 0) {
    // Windows scancodes are only 8-bit, so store the low-order byte into the
    // event and set the extended flag if any high-order bits are set. The only
    // high-order values we should see are 0xE0 or 0xE1. The extended bit
    // usually distinguishes keys with the same meaning, e.g. left & right
    // shift.
    input.ki.wScan &= 0xFF;
    if ((scancode & 0xFF00) != 0x0000)
      input.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
  }

  if (SendInput(1, &input, sizeof(INPUT)) == 0)
    PLOG(ERROR) << "Failed to inject a key event";
}

using protocol::ClipboardEvent;
using protocol::KeyEvent;
using protocol::TextEvent;
using protocol::MouseEvent;

// A class to generate events on Windows.
class InputInjectorWin : public InputInjector {
 public:
  InputInjectorWin(scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
                   scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner);
  virtual ~InputInjectorWin();

  // ClipboardStub interface.
  virtual void InjectClipboardEvent(const ClipboardEvent& event) OVERRIDE;

  // InputStub interface.
  virtual void InjectKeyEvent(const KeyEvent& event) OVERRIDE;
  virtual void InjectTextEvent(const TextEvent& event) OVERRIDE;
  virtual void InjectMouseEvent(const MouseEvent& event) OVERRIDE;

  // InputInjector interface.
  virtual void Start(
      scoped_ptr<protocol::ClipboardStub> client_clipboard) OVERRIDE;

 private:
  // The actual implementation resides in InputInjectorWin::Core class.
  class Core : public base::RefCountedThreadSafe<Core> {
   public:
    Core(scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
         scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner);

    // Mirrors the ClipboardStub interface.
    void InjectClipboardEvent(const ClipboardEvent& event);

    // Mirrors the InputStub interface.
    void InjectKeyEvent(const KeyEvent& event);
    void InjectTextEvent(const TextEvent& event);
    void InjectMouseEvent(const MouseEvent& event);

    // Mirrors the InputInjector interface.
    void Start(scoped_ptr<protocol::ClipboardStub> client_clipboard);

    void Stop();

   private:
    friend class base::RefCountedThreadSafe<Core>;
    virtual ~Core();

    void HandleKey(const KeyEvent& event);
    void HandleText(const TextEvent& event);
    void HandleMouse(const MouseEvent& event);

    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;
    scoped_ptr<Clipboard> clipboard_;

    DISALLOW_COPY_AND_ASSIGN(Core);
  };

  scoped_refptr<Core> core_;

  DISALLOW_COPY_AND_ASSIGN(InputInjectorWin);
};

InputInjectorWin::InputInjectorWin(
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner) {
  core_ = new Core(main_task_runner, ui_task_runner);
}

InputInjectorWin::~InputInjectorWin() {
  core_->Stop();
}

void InputInjectorWin::InjectClipboardEvent(const ClipboardEvent& event) {
  core_->InjectClipboardEvent(event);
}

void InputInjectorWin::InjectKeyEvent(const KeyEvent& event) {
  core_->InjectKeyEvent(event);
}

void InputInjectorWin::InjectTextEvent(const TextEvent& event) {
  core_->InjectTextEvent(event);
}

void InputInjectorWin::InjectMouseEvent(const MouseEvent& event) {
  core_->InjectMouseEvent(event);
}

void InputInjectorWin::Start(
    scoped_ptr<protocol::ClipboardStub> client_clipboard) {
  core_->Start(client_clipboard.Pass());
}

InputInjectorWin::Core::Core(
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner)
    : main_task_runner_(main_task_runner),
      ui_task_runner_(ui_task_runner),
      clipboard_(Clipboard::Create()) {
}

void InputInjectorWin::Core::InjectClipboardEvent(const ClipboardEvent& event) {
  if (!ui_task_runner_->BelongsToCurrentThread()) {
    ui_task_runner_->PostTask(
        FROM_HERE, base::Bind(&Core::InjectClipboardEvent, this, event));
    return;
  }

  // |clipboard_| will ignore unknown MIME-types, and verify the data's format.
  clipboard_->InjectClipboardEvent(event);
}

void InputInjectorWin::Core::InjectKeyEvent(const KeyEvent& event) {
  if (!main_task_runner_->BelongsToCurrentThread()) {
    main_task_runner_->PostTask(FROM_HERE,
                                base::Bind(&Core::InjectKeyEvent, this, event));
    return;
  }

  HandleKey(event);
}

void InputInjectorWin::Core::InjectTextEvent(const TextEvent& event) {
  if (!main_task_runner_->BelongsToCurrentThread()) {
    main_task_runner_->PostTask(
        FROM_HERE, base::Bind(&Core::InjectTextEvent, this, event));
    return;
  }

  HandleText(event);
}

void InputInjectorWin::Core::InjectMouseEvent(const MouseEvent& event) {
  if (!main_task_runner_->BelongsToCurrentThread()) {
    main_task_runner_->PostTask(
        FROM_HERE, base::Bind(&Core::InjectMouseEvent, this, event));
    return;
  }

  HandleMouse(event);
}

void InputInjectorWin::Core::Start(
    scoped_ptr<protocol::ClipboardStub> client_clipboard) {
  if (!ui_task_runner_->BelongsToCurrentThread()) {
    ui_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&Core::Start, this, base::Passed(&client_clipboard)));
    return;
  }

  clipboard_->Start(client_clipboard.Pass());
}

void InputInjectorWin::Core::Stop() {
  if (!ui_task_runner_->BelongsToCurrentThread()) {
    ui_task_runner_->PostTask(FROM_HERE, base::Bind(&Core::Stop, this));
    return;
  }

  clipboard_->Stop();
}

InputInjectorWin::Core::~Core() {}

void InputInjectorWin::Core::HandleKey(const KeyEvent& event) {
  // HostEventDispatcher should filter events missing the pressed field.
  DCHECK(event.has_pressed() && event.has_usb_keycode());

  // Reset the system idle suspend timeout.
  SetThreadExecutionState(ES_SYSTEM_REQUIRED);

  int scancode =
      ui::KeycodeConverter::UsbKeycodeToNativeKeycode(event.usb_keycode());
  VLOG(3) << "Converting USB keycode: " << std::hex << event.usb_keycode()
          << " to scancode: " << scancode << std::dec;

  // Ignore events which can't be mapped.
  if (scancode == ui::KeycodeConverter::InvalidNativeKeycode())
    return;

  uint32_t flags = KEYEVENTF_SCANCODE | (event.pressed() ? 0 : KEYEVENTF_KEYUP);
  SendKeyboardInput(flags, scancode);
}

void InputInjectorWin::Core::HandleText(const TextEvent& event) {
  // HostEventDispatcher should filter events missing the pressed field.
  DCHECK(event.has_text());

  base::string16 text = base::UTF8ToUTF16(event.text());
  for (base::string16::const_iterator it = text.begin();
       it != text.end(); ++it)  {
    SendKeyboardInput(KEYEVENTF_UNICODE, *it);
    SendKeyboardInput(KEYEVENTF_UNICODE | KEYEVENTF_KEYUP, *it);
  }
}

void InputInjectorWin::Core::HandleMouse(const MouseEvent& event) {
  // Reset the system idle suspend timeout.
  SetThreadExecutionState(ES_SYSTEM_REQUIRED);

  INPUT input;
  memset(&input, 0, sizeof(input));
  input.type = INPUT_MOUSE;

  if (event.has_delta_x() && event.has_delta_y()) {
    input.mi.dx = event.delta_x();
    input.mi.dy = event.delta_y();
    input.mi.dwFlags |= MOUSEEVENTF_MOVE | MOUSEEVENTF_VIRTUALDESK;
  } else if (event.has_x() && event.has_y()) {
    int width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    if (width > 1 && height > 1) {
      int x = std::max(0, std::min(width, event.x()));
      int y = std::max(0, std::min(height, event.y()));
      input.mi.dx = static_cast<int>((x * 65535) / (width - 1));
      input.mi.dy = static_cast<int>((y * 65535) / (height - 1));
      input.mi.dwFlags |=
          MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;
    }
  }

  int wheel_delta_x = 0;
  int wheel_delta_y = 0;
  if (event.has_wheel_delta_x() && event.has_wheel_delta_y()) {
    wheel_delta_x = static_cast<int>(event.wheel_delta_x());
    wheel_delta_y = static_cast<int>(event.wheel_delta_y());
  }

  if (wheel_delta_x != 0 || wheel_delta_y != 0) {
    if (wheel_delta_x != 0) {
      input.mi.mouseData = wheel_delta_x;
      input.mi.dwFlags |= MOUSEEVENTF_HWHEEL;
    }
    if (wheel_delta_y != 0) {
      input.mi.mouseData = wheel_delta_y;
      input.mi.dwFlags |= MOUSEEVENTF_WHEEL;
    }
  }

  if (event.has_button() && event.has_button_down()) {
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
      input.mi.dwFlags |= down ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
    } else if (button == MouseEvent::BUTTON_MIDDLE) {
      input.mi.dwFlags |= down ? MOUSEEVENTF_MIDDLEDOWN : MOUSEEVENTF_MIDDLEUP;
    } else if (button == MouseEvent::BUTTON_RIGHT) {
      input.mi.dwFlags |= down ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP;
    } else {
      input.mi.dwFlags |= down ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
    }
  }

  if (input.mi.dwFlags) {
    if (SendInput(1, &input, sizeof(INPUT)) == 0)
      PLOG(ERROR) << "Failed to inject a mouse event";
  }
}

}  // namespace

scoped_ptr<InputInjector> InputInjector::Create(
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner) {
  return scoped_ptr<InputInjector>(
      new InputInjectorWin(main_task_runner, ui_task_runner));
}

}  // namespace remoting
