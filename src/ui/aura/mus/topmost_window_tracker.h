// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_MUS_TOPMOST_WINDOW_TRACKER_H_
#define UI_AURA_MUS_TOPMOST_WINDOW_TRACKER_H_

#include <vector>

#include "base/macros.h"
#include "ui/aura/aura_export.h"

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

  aura::Window* topmost() { return topmost_; }
  aura::Window* second_topmost() { return second_topmost_; }

 private:
  friend class WindowTreeClient;

  // Updates the topmost and the real_topmost windows to the specified ones.
  // Will be called by WindowTreeClient.
  void OnTopmostWindowChanged(const std::vector<WindowMus*> topmosts);

  WindowTreeClient* client_;

  // The root window for the topmost window under the current cursor position.
  // This can be the event target.
  aura::Window* topmost_ = nullptr;

  // Another root window for the topmost window under the current cursor
  // position. This is the second topmost window if the topmost window happens
  // to be the current event target. If the topmost window isn't the event
  // target, this will point to the same topmost window.
  aura::Window* second_topmost_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(TopmostWindowTracker);
};

}  // namespace aura

#endif  // UI_AURA_MUS_TOPMOST_WINDOW_TRACKER_H_
