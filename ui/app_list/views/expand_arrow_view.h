// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_VIEWS_EXPAND_ARROW_VIEW_H_
#define UI_APP_LIST_VIEWS_EXPAND_ARROW_VIEW_H_

#include "ui/views/controls/button/button.h"

namespace views {
class ImageView;
class InkDrop;
class InkDropMask;
class InkDropRipple;
class InkDropHighlight;
}  // namespace views

namespace app_list {

class AppListView;
class ContentsView;

// A tile item for the expand arrow on the start page.
class ExpandArrowView : public views::Button, public views::ButtonListener {
 public:
  ExpandArrowView(ContentsView* contents_view, AppListView* app_list_view);
  ~ExpandArrowView() override;

  bool selected() { return selected_; }
  void SetSelected(bool selected);

  // Overridden from views::Button:
  void PaintButtonContents(gfx::Canvas* canvas) override;

  // Overridden from views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // Overridden from views::View:
  gfx::Size CalculatePreferredSize() const override;
  void Layout() override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;

  // Overridden from views::InkDropHost:
  std::unique_ptr<views::InkDrop> CreateInkDrop() override;
  std::unique_ptr<views::InkDropMask> CreateInkDropMask() const override;
  std::unique_ptr<views::InkDropRipple> CreateInkDropRipple() const override;
  std::unique_ptr<views::InkDropHighlight> CreateInkDropHighlight()
      const override;

 private:
  void TransitToFullscreenAllAppsState();

  ContentsView* const contents_view_;
  AppListView* const app_list_view_;  // Owned by the views hierarchy.

  views::ImageView* icon_;

  bool selected_ = false;

  DISALLOW_COPY_AND_ASSIGN(ExpandArrowView);
};

}  // namespace app_list

#endif  // UI_APP_LIST_VIEWS_EXPAND_ARROW_VIEW_H_
