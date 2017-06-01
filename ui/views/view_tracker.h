// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_VIEW_TRACKER_H_
#define UI_VIEWS_VIEW_TRACKER_H_

#include <vector>

#include "base/macros.h"
#include "ui/views/view_observer.h"
#include "ui/views/views_export.h"

namespace views {

// ViewTracker tracks a set of Views. When a View is deleted the View is removed
// from the list of tracked views.
//
// The behavior of this matches WindowTracker, see it for specifics.
class VIEWS_EXPORT ViewTracker : public ViewObserver {
 public:
  ViewTracker();
  ~ViewTracker() override;

  void Add(View* view);
  void Remove(View* view);

  const std::vector<View*> views() const { return views_; }

  // ViewObserver:
  void OnViewIsDeleting(View* observed_view) override;

 private:
  std::vector<View*> views_;

  DISALLOW_COPY_AND_ASSIGN(ViewTracker);
};

}  // namespace views

#endif  // UI_VIEWS_VIEW_TRACKER_H_
