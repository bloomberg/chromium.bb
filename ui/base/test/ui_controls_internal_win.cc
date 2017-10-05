// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/test/ui_controls_internal_win.h"

#include <cmath>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
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
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&InputDispatcher::NotifyTask, this));
}

void InputDispatcher::NotifyTask() {
  task_.Run();
  Release();
}

// Private functions ----------------------------------------------------------

UINT MapVirtualKeyToScanCode(UINT code) {
  UINT ret_code = MapVirtualKey(code, MAPVK_VK_TO_VSC);
  // We have to manually mark the following virtual
  // keys as extended or else their scancodes depend
  // on NumLock state.
  // For ex. VK_DOWN will be mapped onto either DOWN or NumPad2
  // depending on NumLock state which can lead to tests failures.
  switch (code) {
    case VK_INSERT:
    case VK_DELETE:
    case VK_HOME:
    case VK_END:
    case VK_NEXT:
    case VK_PRIOR:
    case VK_LEFT:
    case VK_RIGHT:
    case VK_UP:
    case VK_DOWN:
    case VK_NUMLOCK:
      ret_code |= KF_EXTENDED;
    default:
      break;
  }
  return ret_code;
}

// Whether scan code should be used for |key|.
// When sending keyboard events by SendInput() function, Windows does not
// "smartly" add scan code if virtual key-code is used. So these key events
// won't have scan code or DOM UI Event code string.
// But we cannot blindly send all events with scan code. For some layout
// dependent keys, the Windows may not translate them to what they used to be,
// because the test cases are usually running in headless environment with
// default keyboard layout. So fall back to use virtual key code for these keys.
bool ShouldSendThroughScanCode(ui::KeyboardCode key) {
  const DWORD native_code = ui::WindowsKeyCodeForKeyboardCode(key);
  const DWORD scan_code = MapVirtualKeyToScanCode(native_code);
  return native_code == MapVirtualKey(scan_code, MAPVK_VSC_TO_VK);
}

// Populate the INPUT structure with the appropriate keyboard event
// parameters required by SendInput
bool FillKeyboardInput(ui::KeyboardCode key, INPUT* input, bool key_up) {
  memset(input, 0, sizeof(INPUT));
  input->type = INPUT_KEYBOARD;
  input->ki.wVk = ui::WindowsKeyCodeForKeyboardCode(key);
  if (ShouldSendThroughScanCode(key)) {
    input->ki.wScan = MapVirtualKeyToScanCode(input->ki.wVk);
    // When KEYEVENTF_SCANCODE is used, ki.wVk is ignored, so we do not need to
    // clear it.
    input->ki.dwFlags = KEYEVENTF_SCANCODE;
    if ((input->ki.wScan & 0xFF00) != 0)
      input->ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
  }
  if (key_up)
    input->ki.dwFlags |= KEYEVENTF_KEYUP;

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

  INPUT input[8] = {};  // 8, assuming all the modifiers are activated.

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
    if (task)
      base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, task);
    return true;
  }

  // Get the max screen coordinate for use in computing the normalized absolute
  // coordinates required by SendInput.
  int max_x = ::GetSystemMetrics(SM_CXSCREEN) - 1;
  int max_y = ::GetSystemMetrics(SM_CYSCREEN) - 1;

  // Clamp the inputs.
  if (screen_x < 0)
    screen_x = 0;
  else if (screen_x > max_x)
    screen_x = max_x;
  if (screen_y < 0)
    screen_y = 0;
  else if (screen_y > max_y)
    screen_y = max_y;

  // Form the input data containing the normalized absolute coordinates.
  INPUT input = {INPUT_MOUSE};
  input.mi.dx = static_cast<LONG>(std::ceil(screen_x * (65535.0 / max_x)));
  input.mi.dy = static_cast<LONG>(std::ceil(screen_y * (65535.0 / max_y)));
  input.mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE;

  scoped_refptr<InputDispatcher> dispatcher;
  if (task)
    dispatcher = base::MakeRefCounted<InputDispatcher>(task, WM_MOUSEMOVE);

  if (!::SendInput(1, &input, sizeof(input)))
    return false;

  if (dispatcher)
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

bool SendTouchEventsImpl(int action, int num, int x, int y) {
  const int kTouchesLengthCap = 16;
  DCHECK_LE(num, kTouchesLengthCap);

  using InitializeTouchInjectionFn = BOOL(WINAPI*)(UINT32, DWORD);
  static InitializeTouchInjectionFn initialize_touch_injection =
      reinterpret_cast<InitializeTouchInjectionFn>(GetProcAddress(
          GetModuleHandleA("user32.dll"), "InitializeTouchInjection"));
  if (!initialize_touch_injection ||
      !initialize_touch_injection(num, TOUCH_FEEDBACK_INDIRECT)) {
    return false;
  }

  using InjectTouchInputFn = BOOL(WINAPI*)(UINT32, POINTER_TOUCH_INFO*);
  static InjectTouchInputFn inject_touch_input =
      reinterpret_cast<InjectTouchInputFn>(
          GetProcAddress(GetModuleHandleA("user32.dll"), "InjectTouchInput"));
  if (!inject_touch_input)
    return false;

  POINTER_TOUCH_INFO pointer_touch_info[kTouchesLengthCap];
  for (int i = 0; i < num; i++) {
    POINTER_TOUCH_INFO& contact = pointer_touch_info[i];
    memset(&contact, 0, sizeof(POINTER_TOUCH_INFO));
    contact.pointerInfo.pointerType = PT_TOUCH;
    contact.pointerInfo.pointerId = i;
    contact.pointerInfo.ptPixelLocation.y = y;
    contact.pointerInfo.ptPixelLocation.x = x + 10 * i;

    contact.touchFlags = TOUCH_FLAG_NONE;
    contact.touchMask =
        TOUCH_MASK_CONTACTAREA | TOUCH_MASK_ORIENTATION | TOUCH_MASK_PRESSURE;
    contact.orientation = 90;
    contact.pressure = 32000;

    // defining contact area
    contact.rcContact.top = contact.pointerInfo.ptPixelLocation.y - 2;
    contact.rcContact.bottom = contact.pointerInfo.ptPixelLocation.y + 2;
    contact.rcContact.left = contact.pointerInfo.ptPixelLocation.x - 2;
    contact.rcContact.right = contact.pointerInfo.ptPixelLocation.x + 2;

    contact.pointerInfo.pointerFlags =
        POINTER_FLAG_DOWN | POINTER_FLAG_INRANGE | POINTER_FLAG_INCONTACT;
  }
  // Injecting the touch down on screen
  if (!inject_touch_input(num, pointer_touch_info))
    return false;

  // Injecting the touch move on screen
  if (action & MOVE) {
    for (int i = 0; i < num; i++) {
      POINTER_TOUCH_INFO& contact = pointer_touch_info[i];
      contact.pointerInfo.ptPixelLocation.y = y + 10;
      contact.pointerInfo.ptPixelLocation.x = x + 10 * i + 30;
      contact.pointerInfo.pointerFlags =
          POINTER_FLAG_UPDATE | POINTER_FLAG_INRANGE | POINTER_FLAG_INCONTACT;
    }
    if (!inject_touch_input(num, pointer_touch_info))
      return false;
  }

  // Injecting the touch up on screen
  if (action & RELEASE) {
    for (int i = 0; i < num; i++) {
      POINTER_TOUCH_INFO& contact = pointer_touch_info[i];
      contact.pointerInfo.ptPixelLocation.y = y + 10;
      contact.pointerInfo.ptPixelLocation.x = x + 10 * i + 30;
      contact.pointerInfo.pointerFlags = POINTER_FLAG_UP | POINTER_FLAG_INRANGE;
    }
    if (!inject_touch_input(num, pointer_touch_info))
      return false;
  }

  return true;
}

}  // namespace internal
}  // namespace ui_controls
