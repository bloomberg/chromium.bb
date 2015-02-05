// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_CHROMEOS_IME_KEYBOARD_X11_H_
#define UI_BASE_IME_CHROMEOS_IME_KEYBOARD_X11_H_

#include "ui/base/ime/chromeos/ime_keyboard.h"

#include <cstdlib>
#include <cstring>
#include <queue>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "base/process/process_handle.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/sys_info.h"
#include "base/threading/thread_checker.h"
#include "ui/base/ime/ui_base_ime_export.h"

namespace chromeos {
namespace input_method {

class UI_BASE_IME_EXPORT ImeKeyboardX11 : public ImeKeyboard {
 public:
  ImeKeyboardX11();
  ~ImeKeyboardX11() override;

  // ImeKeyboard:
  bool SetCurrentKeyboardLayoutByName(const std::string& layout_name) override;
  bool ReapplyCurrentKeyboardLayout() override;
  void ReapplyCurrentModifierLockStatus() override;
  void DisableNumLock() override;
  void SetCapsLockEnabled(bool enable_caps_lock) override;
  bool CapsLockIsEnabled() override;
  bool SetAutoRepeatEnabled(bool enabled) override;
  bool GetAutoRepeatEnabled() override;
  bool SetAutoRepeatRate(const AutoRepeatRate& rate) override;

 private:
  // Returns a mask for Num Lock (e.g. 1U << 4). Returns 0 on error.
  unsigned int GetNumLockMask();

  // Sets the caps-lock status. Note that calling this function always disables
  // the num-lock.
  void SetLockedModifiers();

  // This function is used by SetLayout() and RemapModifierKeys(). Calls
  // setxkbmap command if needed, and updates the last_full_layout_name_ cache.
  bool SetLayoutInternal(const std::string& layout_name, bool force);

  // Executes 'setxkbmap -layout ...' command asynchronously using a layout name
  // in the |execute_queue_|. Do nothing if the queue is empty.
  // TODO(yusukes): Use libxkbfile.so instead of the command (crosbug.com/13105)
  void MaybeExecuteSetLayoutCommand();

  // Polls to see setxkbmap process exits.
  void PollUntilChildFinish(const base::ProcessHandle handle);

  // Called when execve'd setxkbmap process exits.
  void OnSetLayoutFinish();

  const bool is_running_on_chrome_os_;
  unsigned int num_lock_mask_;

  // A queue for executing setxkbmap one by one.
  std::queue<std::string> execute_queue_;

  base::ThreadChecker thread_checker_;

  base::WeakPtrFactory<ImeKeyboardX11> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ImeKeyboardX11);
};


} // namespace input_method
} // namespace chromeos

#endif  // UI_BASE_IME_CHROMEOS_IME_KEYBOARD_X11_H_
