// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_MENU_MENU_SCROLL_VIEW_CONTAINER_H_
#define UI_VIEWS_CONTROLS_MENU_MENU_SCROLL_VIEW_CONTAINER_H_

#include "base/memory/raw_ptr.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/controls/menu/menu_types.h"
#include "ui/views/view.h"

namespace views {

class MenuItemView;
class SubmenuView;

// MenuScrollViewContainer contains the SubmenuView (through a MenuScrollView)
// and two scroll buttons. The scroll buttons are only visible and enabled if
// the preferred height of the SubmenuView is bigger than our bounds.
class MenuScrollViewContainer : public View {
 public:
  METADATA_HEADER(MenuScrollViewContainer);

  explicit MenuScrollViewContainer(SubmenuView* content_view);

  MenuScrollViewContainer(const MenuScrollViewContainer&) = delete;
  MenuScrollViewContainer& operator=(const MenuScrollViewContainer&) = delete;

  // Returns the buttons for scrolling up/down.
  View* scroll_down_button() const { return scroll_down_button_; }
  View* scroll_up_button() const { return scroll_up_button_; }

  // External function to check if the bubble border is used.
  bool HasBubbleBorder() const;

  // View overrides.
  gfx::Size CalculatePreferredSize() const override;
  void OnPaintBackground(gfx::Canvas* canvas) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void OnThemeChanged() override;

 protected:
  // View override.
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;

 private:
  friend class MenuScrollView;

  void DidScrollToTop();
  void DidScrollToBottom();
  void DidScrollAwayFromTop();
  void DidScrollAwayFromBottom();

  // Create a default border or bubble border, as appropriate.
  void CreateBorder();

  // Create the default border.
  void CreateDefaultBorder();

  // Create the bubble border.
  void CreateBubbleBorder();

  BubbleBorder::Arrow BubbleBorderTypeFromAnchor(MenuAnchorPosition anchor);

  // Returns the last item in the menu if it is of type HIGHLIGHTED.
  MenuItemView* GetFootnote() const;

  class MenuScrollView;

  // The scroll buttons.
  raw_ptr<View> scroll_up_button_;
  raw_ptr<View> scroll_down_button_;

  // The scroll view.
  raw_ptr<MenuScrollView> scroll_view_;

  // The content view.
  raw_ptr<SubmenuView> content_view_;

  // If set the currently set border is a bubble border.
  BubbleBorder::Arrow arrow_ = BubbleBorder::NONE;

  // Corner radius of the background.
  int corner_radius_ = 0;
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_MENU_MENU_SCROLL_VIEW_CONTAINER_H_
