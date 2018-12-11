// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_ACCELERATORS_GLOBAL_MEDIA_KEYS_LISTENER_WIN_H_
#define UI_BASE_ACCELERATORS_GLOBAL_MEDIA_KEYS_LISTENER_WIN_H_

#include "base/threading/thread.h"
#include "ui/base/accelerators/media_keys_listener.h"
#include "ui/base/ui_base_export.h"

namespace ui {

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
  void StartWatchingMediaKeys() override;
  void StopWatchingMediaKeys() override;
  bool IsWatchingMediaKeys() const override;

 private:
  void EnsureHookOnInternalThread();
  void DeleteHookOnInternalThread();
  void KeyEventCallbackOnInternalThread(KeyEvent* event);

  static bool has_instance_;

  MediaKeysListener::Delegate* delegate_;
  std::unique_ptr<KeyboardHook> hook_;
  base::Thread thread_;

  DISALLOW_COPY_AND_ASSIGN(GlobalMediaKeysListenerWin);
};

}  // namespace ui

#endif  // UI_BASE_ACCELERATORS_GLOBAL_MEDIA_KEYS_LISTENER_WIN_H_
