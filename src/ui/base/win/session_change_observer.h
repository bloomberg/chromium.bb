// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_WIN_SESSION_CHANGE_OBSERVER_H_
#define UI_BASE_WIN_SESSION_CHANGE_OBSERVER_H_

#include <windows.h>

#include "base/callback.h"
#include "base/macros.h"
#include "ui/base/ui_base_export.h"

namespace ui {

// Calls the provided callback on WM_WTSSESSION_CHANGE messages along with
// managing the tricky business of observing a singleton object.
class UI_BASE_EXPORT SessionChangeObserver {
 public:
  // WPARAM is the wparam passed to the OnWndProc when message is
  // WM_WTSSESSION_CHANGE.
  typedef base::RepeatingCallback<void(WPARAM)> WtsCallback;
  explicit SessionChangeObserver(const WtsCallback& callback);
  ~SessionChangeObserver();

 private:
  class WtsRegistrationNotificationManager;

  void OnSessionChange(WPARAM wparam);
  void ClearCallback();

  WtsCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(SessionChangeObserver);
};

}  // namespace ui

#endif  // UI_BASE_WIN_SESSION_CHANGE_OBSERVER_H_
