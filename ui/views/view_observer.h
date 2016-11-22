// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_VIEW_OBSERVER_H_
#define UI_VIEWS_VIEW_OBSERVER_H_

#include "ui/views/views_export.h"

namespace views {

class View;

// Observer can listen to various events on the Views.
class VIEWS_EXPORT ViewObserver {
 public:
  // Called when |child| is added to its parent. Invoked on the parent which has
  // the |child| added to it.
  virtual void OnChildViewAdded(View* child) {}

  // Called when |child| is removed from its |parent|.
  virtual void OnChildViewRemoved(View* child, View* parent) {}

  // Called when View::SetVisible() is called with a new value.
  // The |view| may still be hidden at this point.
  virtual void OnViewVisibilityChanged(View* view) {}

  virtual void OnViewEnabledChanged(View* view) {}

  virtual void OnViewBoundsChanged(View* view) {}

  // Invoked whenever a child is moved to another index. This is called on the
  // parent view. |view| is the child view being moved.
  virtual void OnChildViewReordered(View* view) {}

 protected:
  virtual ~ViewObserver() {}
};

}  // namespace views

#endif  // UI_VIEWS_VIEW_OBSERVER_H_
