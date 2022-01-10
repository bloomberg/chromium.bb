// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_TAB_STRIP_REGION_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_TAB_STRIP_REGION_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/views/accessible_pane_view.h"

namespace views {
class FlexLayout;
}

class NewTabButton;
class TabSearchButton;
class TabStrip;
class TipMarqueeView;

// Container for the tabstrip and the other views sharing space with it -
// with the exception of the caption buttons.
class TabStripRegionView final : public views::AccessiblePaneView {
 public:
  METADATA_HEADER(TabStripRegionView);
  explicit TabStripRegionView(std::unique_ptr<TabStrip> tab_strip);
  TabStripRegionView(const TabStripRegionView&) = delete;
  TabStripRegionView& operator=(const TabStripRegionView&) = delete;
  ~TabStripRegionView() override;

  // Returns true if the specified rect intersects the window caption area of
  // the browser window. |rect| is in the local coordinate space
  // of |this|.
  bool IsRectInWindowCaption(const gfx::Rect& rect);

  // A convenience function which calls |IsRectInWindowCaption()| with a rect of
  // size 1x1 and an origin of |point|. |point| is in the local coordinate space
  // of |this|.
  bool IsPositionInWindowCaption(const gfx::Point& point);

  // Called when the colors of the frame change.
  void FrameColorsChanged();

  NewTabButton* new_tab_button() { return new_tab_button_; }

  TabSearchButton* tab_search_button() { return tab_search_button_; }

  TipMarqueeView* tip_marquee_view() { return tip_marquee_view_; }

  views::View* reserved_grab_handle_space_for_testing() {
    return reserved_grab_handle_space_;
  }

  // views::AccessiblePaneView:
  void ChildPreferredSizeChanged(views::View* child) override;
  gfx::Size GetMinimumSize() const override;
  void OnThemeChanged() override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  views::View* GetDefaultFocusableChild() override;

  views::FlexLayout* layout_manager_for_testing() { return layout_manager_; }

 private:
  // Updates the border padding for |new_tab_button_|.  This should be called
  // whenever any input of the computation of the border's sizing changes.
  void UpdateNewTabButtonBorder();

  raw_ptr<views::FlexLayout> layout_manager_ = nullptr;
  raw_ptr<views::View> tab_strip_container_;
  raw_ptr<views::View> reserved_grab_handle_space_;
  raw_ptr<TabStrip> tab_strip_;
  raw_ptr<NewTabButton> new_tab_button_ = nullptr;
  raw_ptr<TabSearchButton> tab_search_button_ = nullptr;
  raw_ptr<TipMarqueeView> tip_marquee_view_ = nullptr;

  const base::CallbackListSubscription subscription_ =
      ui::TouchUiController::Get()->RegisterCallback(
          base::BindRepeating(&TabStripRegionView::UpdateNewTabButtonBorder,
                              base::Unretained(this)));
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_TAB_STRIP_REGION_VIEW_H_
