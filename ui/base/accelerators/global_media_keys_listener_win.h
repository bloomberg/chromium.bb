// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_ACCELERATORS_GLOBAL_MEDIA_KEYS_LISTENER_WIN_H_
#define UI_BASE_ACCELERATORS_GLOBAL_MEDIA_KEYS_LISTENER_WIN_H_

#include "base/containers/flat_set.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread.h"
#include "ui/base/accelerators/media_keys_listener.h"
#include "ui/base/ui_base_export.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace ui {

class Accelerator;
class KeyboardHook;
class KeyEvent;

// Implementation of MediaKeysListener that uses a KeyboardHook to globally
// listen for media key presses. It only allows for a single instance to be
// created in order to prevent multiple low-level keyboard hooks from being
// installed on a system. It also runs the hook on a dedicated thread to help
// reduce keyboard input delays.
class UI_BASE_EXPORT GlobalMediaKeysListenerWin : public MediaKeysListener {
 public:
  explicit GlobalMediaKeysListenerWin(MediaKeysListener::Delegate* delegate);
  ~GlobalMediaKeysListenerWin() override;

  static bool has_instance() { return has_instance_; }

  // MediaKeysListener implementation.
  void StartWatchingMediaKey(KeyboardCode key_code) override;
  void StopWatchingMediaKey(KeyboardCode key_code) override;

 private:
  void CreateKeyboardHookIfNecessary();
  void DeleteKeyboardHookIfNecessary();

  // Called on |thread_|.
  void EnsureHookOnInternalThread();
  void DeleteHookOnInternalThread();
  void StartWatchingMediaKeyOnInternalThread(KeyboardCode key_code);
  void StopWatchingMediaKeyOnInternalThread(KeyboardCode key_code);
  void KeyEventCallbackOnInternalThread(KeyEvent* event);

  // Called on |main_thread_task_runner_|.
  void KeyEventCallbackOnMainThread(const Accelerator& accelerator);
  void OnStartedWatchingMediaKeysOnMainThread();

  static bool has_instance_;

  MediaKeysListener::Delegate* delegate_;

  base::Lock key_codes_lock_;
  base::flat_set<KeyboardCode> key_codes_ GUARDED_BY(key_codes_lock_);

  base::Thread thread_;
  std::unique_ptr<KeyboardHook> hook_;

  // Pointer to the task runner of the thread the MediaKeysListener was created
  // on. This is used to run the delegate's callback on the proper thread.
  scoped_refptr<base::SequencedTaskRunner> main_thread_task_runner_;

  base::WeakPtrFactory<GlobalMediaKeysListenerWin> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(GlobalMediaKeysListenerWin);
};

}  // namespace ui

#endif  // UI_BASE_ACCELERATORS_GLOBAL_MEDIA_KEYS_LISTENER_WIN_H_
