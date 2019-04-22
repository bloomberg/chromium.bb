// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_MUS_CURSOR_MANAGER_OWNER_H_
#define UI_VIEWS_MUS_CURSOR_MANAGER_OWNER_H_

#include <memory>

#include "ui/aura/window_tracker.h"
#include "ui/views/mus/mus_export.h"

namespace aura {
class Window;
}

namespace wm {
class CursorManager;
}

namespace views {

// This class owns the cursor manager which can communicate with the window
// server and keeps it as the cursor-client for the specified window as far as
// this class lives.
class VIEWS_MUS_EXPORT CursorManagerOwner {
 public:
  explicit CursorManagerOwner(aura::Window* window);
  ~CursorManagerOwner();

 private:
  aura::WindowTracker tracker_;
  std::unique_ptr<wm::CursorManager> cursor_manager_;

  DISALLOW_COPY_AND_ASSIGN(CursorManagerOwner);
};

}  // namespace views

#endif  // UI_VIEWS_MUS_CURSOR_MANAGER_OWNER_H_
