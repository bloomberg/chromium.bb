// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/test/ui_controls_internal_win.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "ui/events/keycodes/keyboard_code_conversion_win.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace {

// InputDispatcher ------------------------------------------------------------

// InputDispatcher is used to listen for a mouse/keyboard event. When the
// appropriate event is received the task is notified.
class InputDispatcher : public base::RefCounted<InputDispatcher> {
 public:
  InputDispatcher(const base::Closure& task, WPARAM message_waiting_for);

  // Invoked from the hook. If mouse_message matches message_waiting_for_
  // MatchingMessageFound is invoked.
  void DispatchedMessage(WPARAM mouse_message);

  // Invoked when a matching event is found. Uninstalls the hook and schedules
  // an event that notifies the task.
  void MatchingMessageFound();

 private:
  friend class base::RefCounted<InputDispatcher>;

  ~InputDispatcher();

  // Notifies the task and release this (which should delete it).
  void NotifyTask();

  // The task we notify.
  base::Closure task_;

  // Message we're waiting for. Not used for keyboard events.
  const WPARAM message_waiting_for_;

  DISALLOW_COPY_AND_ASSIGN(InputDispatcher);
};

// Have we installed the hook?
bool installed_hook_ = false;

// Return value from SetWindowsHookEx.
HHOOK next_hook_ = NULL;

// If a hook is installed, this is the dispatcher.
InputDispatcher* current_dispatcher_ = NULL;

// Callback from hook when a mouse message is received.
LRESULT CALLBACK MouseHook(int n_code, WPARAM w_param, LPARAM l_param) {
  HHOOK next_hook = next_hook_;
  if (n_code == HC_ACTION) {
    DCHECK(current_dispatcher_);
    current_dispatcher_->DispatchedMessage(w_param);
  }
  return CallNextHookEx(next_hook, n_code, w_param, l_param);
}

// Callback from hook when a key message is received.
LRESULT CALLBACK KeyHook(int n_code, WPARAM w_param, LPARAM l_param) {
  HHOOK next_hook = next_hook_;
  if (n_code == HC_ACTION) {
    DCHECK(current_dispatcher_);
    if (l_param & (1 << 30)) {
      // Only send on key up.
      current_dispatcher_->MatchingMessageFound();
    }
  }
  return CallNextHookEx(next_hook, n_code, w_param, l_param);
}

// Installs dispatcher as the current hook.
void InstallHook(InputDispatcher* dispatcher, bool key_hook) {
  DCHECK(!installed_hook_);
  current_dispatcher_ = dispatcher;
  installed_hook_ = true;
  if (key_hook) {
    next_hook_ = SetWindowsHookEx(WH_KEYBOARD, &KeyHook, NULL,
                                  GetCurrentThreadId());
  } else {
    // NOTE: I originally tried WH_CALLWNDPROCRET, but for some reason I
    // didn't get a mouse message like I do with MouseHook.
    next_hook_ = SetWindowsHookEx(WH_MOUSE, &MouseHook, NULL,
                                  GetCurrentThreadId());
  }
  DCHECK(next_hook_);
}

// Uninstalls the hook set in InstallHook.
void UninstallHook(InputDispatcher* dispatcher) {
  if (current_dispatcher_ == dispatcher) {
    installed_hook_ = false;
    current_dispatcher_ = NULL;
    UnhookWindowsHookEx(next_hook_);
  }
}

InputDispatcher::InputDispatcher(const base::Closure& task,
                                 WPARAM message_waiting_for)
    : task_(task), message_waiting_for_(message_waiting_for) {
  InstallHook(this, message_waiting_for == WM_KEYUP);
}

InputDispatcher::~InputDispatcher() {
  // Make sure the hook isn't installed.
  UninstallHook(this);
}

void InputDispatcher::DispatchedMessage(WPARAM message) {
  if (message == message_waiting_for_)
    MatchingMessageFound();
}

void InputDispatcher::MatchingMessageFound() {
  UninstallHook(this);
  // At the time we're invoked the event has not actually been processed.
  // Use PostTask to make sure the event has been processed before notifying.
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(&InputDispatcher::NotifyTask, this));
}

void InputDispatcher::NotifyTask() {
  task_.Run();
  Release();
}

// Private functions ----------------------------------------------------------

// Populate the INPUT structure with the appropriate keyboard event
// parameters required by SendInput
bool FillKeyboardInput(ui::KeyboardCode key, INPUT* input, bool key_up) {
  memset(input, 0, sizeof(INPUT));
  input->type = INPUT_KEYBOARD;
  input->ki.wVk = ui::WindowsKeyCodeForKeyboardCode(key);
  input->ki.dwFlags = key_up ? KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP :
                               KEYEVENTF_EXTENDEDKEY;

  return true;
}

// Send a key event (up/down)
bool SendKeyEvent(ui::KeyboardCode key, bool up) {
  INPUT input = { 0 };

  if (!FillKeyboardInput(key, &input, up))
    return false;

  if (!::SendInput(1, &input, sizeof(INPUT)))
    return false;

  return true;
}

}  // namespace

namespace ui_controls {
namespace internal {

bool SendKeyPressImpl(HWND window,
                      ui::KeyboardCode key,
                      bool control,
                      bool shift,
                      bool alt,
                      const base::Closure& task) {
  // SendInput only works as we expect it if one of our windows is the
  // foreground window already.
  HWND target_window = (::GetActiveWindow() &&
                        ::GetWindow(::GetActiveWindow(), GW_OWNER) == window) ?
                       ::GetActiveWindow() :
                       window;
  if (window && ::GetForegroundWindow() != target_window)
    return false;

  scoped_refptr<InputDispatcher> dispatcher(
      !task.is_null() ? new InputDispatcher(task, WM_KEYUP) : NULL);

  // If a pop-up menu is open, it won't receive events sent using SendInput.
  // Check for a pop-up menu using its window class (#32768) and if one
  // exists, send the key event directly there.
  HWND popup_menu = ::FindWindow(L"#32768", 0);
  if (popup_menu != NULL && popup_menu == ::GetTopWindow(NULL)) {
    WPARAM w_param = ui::WindowsKeyCodeForKeyboardCode(key);
    LPARAM l_param = 0;
    ::SendMessage(popup_menu, WM_KEYDOWN, w_param, l_param);
    ::SendMessage(popup_menu, WM_KEYUP, w_param, l_param);

    if (dispatcher.get())
      dispatcher->AddRef();
    return true;
  }

  INPUT input[8] = { 0 };  // 8, assuming all the modifiers are activated.

  UINT i = 0;
  if (control) {
    if (!FillKeyboardInput(ui::VKEY_CONTROL, &input[i], false))
      return false;
    i++;
  }

  if (shift) {
    if (!FillKeyboardInput(ui::VKEY_SHIFT, &input[i], false))
      return false;
    i++;
  }

  if (alt) {
    if (!FillKeyboardInput(ui::VKEY_LMENU, &input[i], false))
      return false;
    i++;
  }

  if (!FillKeyboardInput(key, &input[i], false))
    return false;
  i++;

  if (!FillKeyboardInput(key, &input[i], true))
    return false;
  i++;

  if (alt) {
    if (!FillKeyboardInput(ui::VKEY_LMENU, &input[i], true))
      return false;
    i++;
  }

  if (shift) {
    if (!FillKeyboardInput(ui::VKEY_SHIFT, &input[i], true))
      return false;
    i++;
  }

  if (control) {
    if (!FillKeyboardInput(ui::VKEY_CONTROL, &input[i], true))
      return false;
    i++;
  }

  if (::SendInput(i, input, sizeof(INPUT)) != i)
    return false;

  if (dispatcher.get())
    dispatcher->AddRef();

  return true;
}

bool SendMouseMoveImpl(long screen_x,
                       long screen_y,
                       const base::Closure& task) {
  // First check if the mouse is already there.
  POINT current_pos;
  ::GetCursorPos(&current_pos);
  if (screen_x == current_pos.x && screen_y == current_pos.y) {
    if (!task.is_null())
      base::MessageLoop::current()->PostTask(FROM_HERE, task);
    return true;
  }

  INPUT input = { 0 };

  int screen_width = ::GetSystemMetrics(SM_CXSCREEN) - 1;
  int screen_height  = ::GetSystemMetrics(SM_CYSCREEN) - 1;
  LONG pixel_x  = static_cast<LONG>(screen_x * (65535.0f / screen_width));
  LONG pixel_y = static_cast<LONG>(screen_y * (65535.0f / screen_height));

  input.type = INPUT_MOUSE;
  input.mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE;
  input.mi.dx = pixel_x;
  input.mi.dy = pixel_y;

  scoped_refptr<InputDispatcher> dispatcher(
      !task.is_null() ? new InputDispatcher(task, WM_MOUSEMOVE) : NULL);

  if (!::SendInput(1, &input, sizeof(INPUT)))
    return false;

  if (dispatcher.get())
    dispatcher->AddRef();

  return true;
}

bool SendMouseEventsImpl(MouseButton type, int state,
                         const base::Closure& task) {
  DWORD down_flags = MOUSEEVENTF_ABSOLUTE;
  DWORD up_flags = MOUSEEVENTF_ABSOLUTE;
  UINT last_event;

  switch (type) {
    case LEFT:
      down_flags |= MOUSEEVENTF_LEFTDOWN;
      up_flags |= MOUSEEVENTF_LEFTUP;
      last_event = (state & UP) ? WM_LBUTTONUP : WM_LBUTTONDOWN;
      break;

    case MIDDLE:
      down_flags |= MOUSEEVENTF_MIDDLEDOWN;
      up_flags |= MOUSEEVENTF_MIDDLEUP;
      last_event = (state & UP) ? WM_MBUTTONUP : WM_MBUTTONDOWN;
      break;

    case RIGHT:
      down_flags |= MOUSEEVENTF_RIGHTDOWN;
      up_flags |= MOUSEEVENTF_RIGHTUP;
      last_event = (state & UP) ? WM_RBUTTONUP : WM_RBUTTONDOWN;
      break;

    default:
      NOTREACHED();
      return false;
  }

  scoped_refptr<InputDispatcher> dispatcher(
      !task.is_null() ? new InputDispatcher(task, last_event) : NULL);

  INPUT input = { 0 };
  input.type = INPUT_MOUSE;
  input.mi.dwFlags = down_flags;
  if ((state & DOWN) && !::SendInput(1, &input, sizeof(INPUT)))
    return false;

  input.mi.dwFlags = up_flags;
  if ((state & UP) && !::SendInput(1, &input, sizeof(INPUT)))
    return false;

  if (dispatcher.get())
    dispatcher->AddRef();

  return true;
}

}  // namespace internal
}  // namespace ui_controls
