// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/mus/topmost_window_tracker.h"

#include "ui/aura/mus/window_mus.h"
#include "ui/aura/mus/window_tree_client.h"
#include "ui/aura/window.h"

namespace aura {

TopmostWindowTracker::TopmostWindowTracker(WindowTreeClient* client)
    : client_(client), topmost_(std::make_unique<WindowTracker>()) {}

TopmostWindowTracker::~TopmostWindowTracker() {
  client_->StopObservingTopmostWindow();
}

Window* TopmostWindowTracker::GetTopmost() {
  // If |topmost_| is a nullptr, it means there *is* a window but this class
  // does not have access to its pointer. So returns nullptr. See the comment
  // for |topmost_| property for the details.
  if (!topmost_)
    return nullptr;
  // Falls back to the second topmost when the topmost is gone.
  if (topmost_->windows().empty())
    return GetSecondTopmost();
  return topmost_->windows()[0];
}

Window* TopmostWindowTracker::GetSecondTopmost() {
  return second_topmost_.windows().empty() ? nullptr
                                           : second_topmost_.windows()[0];
}

void TopmostWindowTracker::OnTopmostWindowChanged(
    const std::vector<WindowMus*>& topmosts) {
  DCHECK_LE(topmosts.size(), 2u);
  // topmosts can be empty if the mouse/touch event happens outside of the
  // screen. This rarely happens on device but can happen easily when Chrome
  // runs within a Linux desktop. It's fine to just ignore such case.
  if (topmosts.empty())
    return;
  if (topmosts[0]) {
    if (!topmost_)
      topmost_ = std::make_unique<WindowTracker>();
    else
      topmost_->RemoveAll();
    topmost_->Add(topmosts[0]->GetWindow()->GetRootWindow());
  } else {
    topmost_.reset();
  }
  second_topmost_.RemoveAll();
  if (topmosts.size() >= 2 && topmosts[1])
    second_topmost_.Add(topmosts[1]->GetWindow()->GetRootWindow());
}

}  // namespace aura
