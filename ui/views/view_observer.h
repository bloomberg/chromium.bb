// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_VIEW_OBSERVER_H_
#define UI_VIEWS_VIEW_OBSERVER_H_

#include "ui/views/views_export.h"

namespace views {

class View;

// ViewObserver is used to observe changes to a View. The first argument to all
// ViewObserver functions is the View the observer was added to.
class VIEWS_EXPORT ViewObserver {
 public:
  // Called when |child| is added as a child to |observed_view|.
  virtual void OnChildViewAdded(View* observed_view, View* child) {}

  // Called when |child| is removed as a child of |observed_view|.
  virtual void OnChildViewRemoved(View* observed_view, View* child) {}

  // Called when View::SetVisible() is called with a new value. See
  // View::IsDrawn() for details on how visibility and drawn differ.
  virtual void OnViewVisibilityChanged(View* observed_view) {}

  // Called when View::SetEnabled() is called with a new value.
  virtual void OnViewEnabledChanged(View* observed_view) {}

  // Called from View::PreferredSizeChanged().
  virtual void OnViewPreferredSizeChanged(View* observed_view) {}

  // Called when the bounds of |observed_view| change.
  virtual void OnViewBoundsChanged(View* observed_view) {}

  // Called when a child is reordered among its siblings, specifically
  // View::ReorderChildView() is called on |observed_view|.
  virtual void OnChildViewReordered(View* observed_view, View* child) {}

  // Called when the active NativeTheme has changed for |observed_view|.
  virtual void OnViewNativeThemeChanged(View* observed_view) {}

  // Called from ~View.
  virtual void OnViewIsDeleting(View* observed_view) {}

 protected:
  virtual ~ViewObserver() {}
};

}  // namespace views

#endif  // UI_VIEWS_VIEW_OBSERVER_H_
