// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/keyboard_hook_base.h"

#include <utility>

#include <windows.h>

#include "base/logging.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/threading/thread_checker.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/gfx/native_widget_types.h"

namespace ui {

namespace {

// These keys interfere with the return value of ::GetKeyState() as observing
// them directly in LowLevelKeyboardProc will cause their key combinations to be
// ignored.  As an example, the KeyF event in an Alt + F combination will result
// in a missing alt-down flag. Since a regular application can successfully
// receive these keys without using LowLevelKeyboardProc, they can be ignored.
bool IsOSReservedKey(DWORD vk) {
  return vk == VK_SHIFT || vk == VK_LSHIFT || vk == VK_RSHIFT ||
         vk == VK_CONTROL || vk == VK_LCONTROL || vk == VK_RCONTROL ||
         vk == VK_MENU || vk == VK_LMENU || vk == VK_RMENU || vk == VK_LWIN ||
         vk == VK_RWIN || vk == VK_CAPITAL || vk == VK_NUMLOCK ||
         vk == VK_SCROLL;
}

class KeyboardHookWin : public KeyboardHookBase {
 public:
  KeyboardHookWin(base::Optional<base::flat_set<DomCode>> dom_codes,
                  KeyEventCallback callback);
  ~KeyboardHookWin() override;

  bool Register();

 private:
  static LRESULT CALLBACK ProcessKeyEvent(int code,
                                          WPARAM w_param,
                                          LPARAM l_param);
  static KeyboardHookWin* instance_;

  THREAD_CHECKER(thread_checker_);

  HHOOK hook_ = nullptr;

  // Tracks the last key down seen in order to determine if the current key
  // event is a repeated key press.
  DWORD last_key_down_ = 0;

  DISALLOW_COPY_AND_ASSIGN(KeyboardHookWin);
};

// static
KeyboardHookWin* KeyboardHookWin::instance_ = nullptr;

KeyboardHookWin::KeyboardHookWin(
    base::Optional<base::flat_set<DomCode>> dom_codes,
    KeyEventCallback callback)
    : KeyboardHookBase(std::move(dom_codes), std::move(callback)) {}

KeyboardHookWin::~KeyboardHookWin() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  DCHECK_EQ(instance_, this);
  instance_ = nullptr;

  if (!UnhookWindowsHookEx(hook_))
    DPLOG(ERROR) << "UnhookWindowsHookEx failed";
}

bool KeyboardHookWin::Register() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Only one instance of this class can be registered at a time.
  DCHECK(!instance_);
  instance_ = this;

  // Per MSDN this Hook procedure will be called in the context of the thread
  // which installed it.
  hook_ = SetWindowsHookEx(
      WH_KEYBOARD_LL,
      reinterpret_cast<HOOKPROC>(&KeyboardHookWin::ProcessKeyEvent),
      /*hMod=*/nullptr,
      /*dwThreadId=*/0);
  DPLOG_IF(ERROR, !hook_) << "SetWindowsHookEx failed";

  return hook_ != nullptr;
}

// static
LRESULT CALLBACK KeyboardHookWin::ProcessKeyEvent(int code,
                                                  WPARAM w_param,
                                                  LPARAM l_param) {
  // If there is an error unhooking, this method could be called with a null
  // |instance_|.  Ensure we have a valid instance and that |code| is correct
  // before proceeding.
  if (!instance_ || code != HC_ACTION)
    return CallNextHookEx(nullptr, code, w_param, l_param);

  DCHECK_CALLED_ON_VALID_THREAD(instance_->thread_checker_);

  KBDLLHOOKSTRUCT* ll_hooks = reinterpret_cast<KBDLLHOOKSTRUCT*>(l_param);
  DomCode dom_code =
      KeycodeConverter::NativeKeycodeToDomCode(ll_hooks->scanCode);
  if (!IsOSReservedKey(ll_hooks->vkCode) &&
      instance_->ShouldCaptureKeyEvent(dom_code)) {
    MSG msg = {nullptr, w_param, ll_hooks->vkCode,
               (ll_hooks->scanCode << 16) | (ll_hooks->flags & 0xFFFF),
               ll_hooks->time};
    KeyEvent key_event = KeyEventFromMSG(msg);

    // Determine if this key event represents a repeated key event.
    // A low level KB Hook is not passed a parameter or flag which indicates
    // whether the key event is a repeat or not as it is called very early in
    // input handling pipeline. That means we need to apply our own heuristic
    // to determine if it should be treated as a repeated key press or not.
    if (key_event.type() == ET_KEY_PRESSED) {
      if (instance_->last_key_down_ != 0 &&
          instance_->last_key_down_ == ll_hooks->vkCode) {
        key_event.set_flags(key_event.flags() | EF_IS_REPEAT);
      }
      instance_->last_key_down_ = ll_hooks->vkCode;
    } else {
      DCHECK_EQ(key_event.type(), ET_KEY_RELEASED);
      instance_->last_key_down_ = 0;
    }
    instance_->ForwardCapturedKeyEvent(std::make_unique<KeyEvent>(key_event));
    return 1;
  }
  return CallNextHookEx(nullptr, code, w_param, l_param);
}

}  // namespace

// static
std::unique_ptr<KeyboardHook> KeyboardHook::Create(
    base::Optional<base::flat_set<DomCode>> dom_codes,
    gfx::AcceleratedWidget accelerated_widget,
    KeyEventCallback callback) {
  std::unique_ptr<KeyboardHookWin> keyboard_hook =
      std::make_unique<KeyboardHookWin>(std::move(dom_codes),
                                        std::move(callback));

  if (!keyboard_hook->Register())
    return nullptr;

  return keyboard_hook;
}

}  // namespace ui
