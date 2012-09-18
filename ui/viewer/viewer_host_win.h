// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWER_VIEWER_HOST_WIN_H_
#define UI_VIEWER_VIEWER_HOST_WIN_H_

#include "base/memory/scoped_ptr.h"

class ViewerStackingClient;

namespace aura {
class FocusManager;
class RootWindow;

namespace shared {
class RootWindowCaptureClient;
}

}

class ViewerHostWin {
 public:
  ViewerHostWin();
  virtual ~ViewerHostWin();

 private:
  scoped_ptr<aura::RootWindow> root_window_;
  scoped_ptr<aura::shared::RootWindowCaptureClient> root_window_capture_client_;
  scoped_ptr<ViewerStackingClient> stacking_client_;
  scoped_ptr<aura::FocusManager> focus_manager_;

  DISALLOW_COPY_AND_ASSIGN(ViewerHostWin);
};

#endif  // UI_VIEWER_VIEWER_HOST_WIN_H_
