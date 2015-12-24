// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_WINDOW_TRACKER_H_
#define UI_AURA_WINDOW_TRACKER_H_

#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "ui/aura/aura_export.h"
#include "ui/aura/window_observer.h"

namespace aura {

// This class keeps track of a set of windows. A Window is removed either
// explicitly by Remove() or Pop(), or implicitly when the window is destroyed.
class AURA_EXPORT WindowTracker : public WindowObserver {
 public:
  using WindowList = std::vector<Window*>;

  WindowTracker();
  explicit WindowTracker(const WindowList& windows);
  ~WindowTracker() override;

  bool has_windows() const { return !windows_.empty(); }

  // Returns the set of windows being observed.
  const std::vector<Window*>& windows() const { return windows_; }

  // Adds |window| to the set of Windows being tracked.
  void Add(Window* window);

  // Removes |window| from the set of windows being tracked.
  void Remove(Window* window);

  // Returns true if |window| was previously added and has not been removed or
  // deleted.
  bool Contains(Window* window);

  // Removes and returns the window object from the tracking windows.
  aura::Window* Pop();

  // WindowObserver overrides:
  void OnWindowDestroying(Window* window) override;

 private:
  WindowList windows_;

  DISALLOW_COPY_AND_ASSIGN(WindowTracker);
};

}  // namespace aura

#endif  // UI_AURA_WINDOW_TRACKER_H_
