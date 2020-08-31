// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_ARC_APP_WINDOW_DELEGATE_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_ARC_APP_WINDOW_DELEGATE_H_

// Delegate interface for ArcAppWindow.
class ArcAppWindowDelegate {
 public:
  ArcAppWindowDelegate() = default;
  ~ArcAppWindowDelegate() = default;

  ArcAppWindowDelegate(const ArcAppWindowDelegate&) = delete;
  ArcAppWindowDelegate& operator=(const ArcAppWindowDelegate&) = delete;

  // Returns the active task id.
  virtual int GetActiveTaskId() const = 0;
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_ARC_APP_WINDOW_DELEGATE_H_
