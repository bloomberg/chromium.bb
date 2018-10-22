// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/mus/topmost_window_tracker.h"

#include "ui/aura/mus/window_mus.h"
#include "ui/aura/mus/window_tree_client.h"
#include "ui/aura/window.h"

namespace aura {

namespace {

Window* GetRootOrNull(WindowMus* window_mus) {
  return window_mus ? window_mus->GetWindow()->GetRootWindow() : nullptr;
}

}  // namespace

TopmostWindowTracker::TopmostWindowTracker(WindowTreeClient* client)
    : client_(client) {}

TopmostWindowTracker::~TopmostWindowTracker() {
  client_->StopObservingTopmostWindow();
}

void TopmostWindowTracker::OnTopmostWindowChanged(
    const std::vector<WindowMus*> topmosts) {
  DCHECK_LE(topmosts.size(), 2u);
  // topmosts can be empty if the mouse/touch event happens outside of the
  // screen. This rarely happens on device but can happen easily when Chrome
  // runs within a Linux desktop. It's fine to just ignore such case.
  if (topmosts.empty())
    return;
  topmost_ = GetRootOrNull(topmosts[0]);
  second_topmost_ =
      (topmosts.size() > 1) ? GetRootOrNull(topmosts[1]) : topmost_;
}

}  // namespace aura
