// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_MUS_TOPMOST_WINDOW_TRACKER_H_
#define UI_AURA_MUS_TOPMOST_WINDOW_TRACKER_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "ui/aura/aura_export.h"
#include "ui/aura/window_tracker.h"

namespace aura {
class Window;
class WindowMus;
class WindowTreeClient;

// TopmostWindowTracker keeps track of the topmost window under the touch/mouse.
// It is created through WindowTreeClient::StartObservingTopmostWindow().
class AURA_EXPORT TopmostWindowTracker {
 public:
  explicit TopmostWindowTracker(WindowTreeClient* client);
  ~TopmostWindowTracker();

  // Returns the root window for the topmost window under the current cursor
  // position.
  Window* GetTopmost();

  // Returns the root window for the secondary topmost window under the current
  // cursor position.
  Window* GetSecondTopmost();

 private:
  friend class WindowTreeClient;

  // Updates the topmost and the real_topmost windows to the specified ones.
  // Will be called by WindowTreeClient.
  void OnTopmostWindowChanged(const std::vector<WindowMus*>& topmosts);

  WindowTreeClient* client_;

  // The root window for the topmost window under the current cursor position.
  // This can be the event target. Note: this can be empty if there *is* a
  // window but this client doesn't have access to it (i.e. windows in another
  // process). On the other hand, if it has an instance of WindowTracker but the
  // WindowTracker doesn't have any windows in it, this means the topmost window
  // does not exist, or it existed but has been destroyed.
  std::unique_ptr<WindowTracker> topmost_;

  // Another root window for the topmost window under the current cursor
  // position. This is the second topmost window if the topmost window happens
  // to be the current event target. If the topmost window isn't the event
  // target, this will point to nullptr.
  WindowTracker second_topmost_;

  DISALLOW_COPY_AND_ASSIGN(TopmostWindowTracker);
};

}  // namespace aura

#endif  // UI_AURA_MUS_TOPMOST_WINDOW_TRACKER_H_
