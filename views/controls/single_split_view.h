// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_CONTROLS_SINGLE_SPLIT_VIEW_H_
#define VIEWS_CONTROLS_SINGLE_SPLIT_VIEW_H_
#pragma once

#include "base/gtest_prod_util.h"
#include "views/view.h"

namespace views {

// SingleSplitView lays out two views next to each other, either horizontally
// or vertically. A splitter exists between the two views that the user can
// drag around to resize the views.
// Observer's SplitHandleMoved notification helps to monitor user initiated
// layout changes.
class SingleSplitView : public views::View {
 public:
  enum Orientation {
    HORIZONTAL_SPLIT,
    VERTICAL_SPLIT
  };

  class Observer {
   public:
    // Invoked when split handle is moved by the user. |source|'s divider_offset
    // is already set to the new value, but Layout has not happened yet.
    // Returns false if the layout has been handled by the observer, returns
    // true if |source| should do it by itself.
    virtual bool SplitHandleMoved(SingleSplitView* source) = 0;
   protected:
    virtual ~Observer() {}
  };

  SingleSplitView(View* leading,
                  View* trailing,
                  Orientation orientation,
                  Observer* observer);

  virtual void OnBoundsChanged();
  virtual void Layout();

  virtual AccessibilityTypes::Role GetAccessibleRole();

  // SingleSplitView's preferred size is the sum of the preferred widths
  // and the max of the heights.
  virtual gfx::Size GetPreferredSize();

  // Overriden to return a resize cursor when over the divider.
  virtual gfx::NativeCursor GetCursorForPoint(ui::EventType event_type,
                                              const gfx::Point& p);

  Orientation orientation() const {
    return is_horizontal_ ? HORIZONTAL_SPLIT : VERTICAL_SPLIT;
  }

  void set_divider_offset(int divider_offset) {
    divider_offset_ = divider_offset;
  }
  int divider_offset() const { return divider_offset_; }

  // Sets whether the leading component is resized when the split views size
  // changes. The default is true. A value of false results in the trailing
  // component resizing on a bounds change.
  void set_resize_leading_on_bounds_change(bool resize) {
    resize_leading_on_bounds_change_ = resize;
  }

  // Calculates ideal leading and trailing view bounds according to the given
  // split view |bounds|, current divider offset and children visiblity.
  // Does not change children view bounds.
  void CalculateChildrenBounds(const gfx::Rect& bounds,
                               gfx::Rect* leading_bounds,
                               gfx::Rect* trailing_bounds) const;

 protected:
  virtual bool OnMousePressed(const MouseEvent& event);
  virtual bool OnMouseDragged(const MouseEvent& event);
  virtual void OnMouseReleased(const MouseEvent& event, bool canceled);

 private:
  // This test calls OnMouse* functions.
  FRIEND_TEST_ALL_PREFIXES(SingleSplitViewTest, MouseDrag);

  // Returns true if |x| or |y| is over the divider.
  bool IsPointInDivider(const gfx::Point& p);

  // Calculates the new |divider_offset| based on the changes of split view
  // bounds.
  int CalculateDividerOffset(
      int divider_offset,
      const gfx::Rect& previous_bounds,
      const gfx::Rect& new_bounds) const;

  // Returns divider offset within primary axis size range for given split
  // view |bounds|.
  int NormalizeDividerOffset(int divider_offset, const gfx::Rect& bounds) const;

  // Returns width in case of horizontal split and height otherwise.
  int GetPrimaryAxisSize() const {
    return GetPrimaryAxisSize(width(), height());
  }

  int GetPrimaryAxisSize(int h, int v) const {
    return is_horizontal_ ? h : v;
  }

  // Used to track drag info.
  struct DragInfo {
    // The initial coordinate of the mouse when the user started the drag.
    int initial_mouse_offset;
    // The initial position of the divider when the user started the drag.
    int initial_divider_offset;
  };

  DragInfo drag_info_;

  // Orientation of the split view.
  bool is_horizontal_;

  // Position of the divider.
  int divider_offset_;

  // The bounds of the SingleSplitView as a result of the last resize. Used to
  // determine the divider position when a subsequent resize occurs.
  gfx::Rect previous_bounds_;

  bool resize_leading_on_bounds_change_;

  // Observer to notify about user initiated handle movements. Not own by us.
  Observer* observer_;

  DISALLOW_COPY_AND_ASSIGN(SingleSplitView);
};

}  // namespace views

#endif  // VIEWS_CONTROLS_SINGLE_SPLIT_VIEW_H_
