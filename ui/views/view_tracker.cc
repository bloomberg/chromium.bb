// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/view_tracker.h"

#include "base/stl_util.h"
#include "ui/views/view.h"

namespace views {

ViewTracker::ViewTracker() {}

ViewTracker::~ViewTracker() {
  for (View* view : views_)
    view->RemoveObserver(this);
}

void ViewTracker::Add(View* view) {
  if (!view || base::ContainsValue(views_, view))
    return;

  view->AddObserver(this);
  views_.push_back(view);
}

void ViewTracker::Remove(View* view) {
  auto iter = std::find(views_.begin(), views_.end(), view);
  if (iter != views_.end()) {
    view->RemoveObserver(this);
    views_.erase(iter);
  }
}

void ViewTracker::OnViewIsDeleting(View* observed_view) {
  Remove(observed_view);
}

}  // namespace views
