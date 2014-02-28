// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/contents_switcher_view.h"

#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/views/contents_view.h"
#include "ui/gfx/canvas.h"
#include "ui/views/controls/button/custom_button.h"
#include "ui/views/layout/box_layout.h"

namespace app_list {

namespace {

const int kPreferredHeight = 36;

const int kButtonSpacing = 4;
const int kButtonWidth = 36;
const int kButtonHeight = 36;

class ContentsSwitcherButton : public views::CustomButton {
 public:
  explicit ContentsSwitcherButton(views::ButtonListener* listener,
                                  ContentsView::ShowState show_state)
      : views::CustomButton(listener) {
    set_tag(show_state);
  }

  virtual ~ContentsSwitcherButton() {}

  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize() OVERRIDE {
    return gfx::Size(kButtonWidth, kButtonHeight);
  }

  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {
    PaintButton(
        canvas,
        state() == STATE_HOVERED ? kPagerHoverColor : kPagerNormalColor);
  }

 private:
  // Paints a rectangular button.
  void PaintButton(gfx::Canvas* canvas, SkColor base_color) {
    gfx::Rect rect(GetContentsBounds());
    rect.ClampToCenteredSize(gfx::Size(kButtonWidth, kButtonHeight));

    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setStyle(SkPaint::kFill_Style);
    paint.setColor(base_color);
    canvas->DrawRect(rect, paint);
  }

  DISALLOW_COPY_AND_ASSIGN(ContentsSwitcherButton);
};

}  // namespace

ContentsSwitcherView::ContentsSwitcherView(ContentsView* contents_view)
    : contents_view_(contents_view), buttons_(new views::View) {
  AddChildView(buttons_);

  buttons_->SetLayoutManager(new views::BoxLayout(
      views::BoxLayout::kHorizontal, 0, 0, kButtonSpacing));
  buttons_->AddChildView(
      new ContentsSwitcherButton(this, ContentsView::SHOW_APPS));
  buttons_->AddChildView(
      new ContentsSwitcherButton(this, ContentsView::SHOW_SEARCH_RESULTS));
}

ContentsSwitcherView::~ContentsSwitcherView() {}

gfx::Size ContentsSwitcherView::GetPreferredSize() {
  return gfx::Size(buttons_->GetPreferredSize().width(), kPreferredHeight);
}

void ContentsSwitcherView::Layout() {
  gfx::Rect rect(GetContentsBounds());

  // Makes |buttons_| horizontally center and vertically fill.
  gfx::Size buttons_size(buttons_->GetPreferredSize());
  gfx::Rect buttons_bounds(rect.CenterPoint().x() - buttons_size.width() / 2,
                           rect.y(),
                           buttons_size.width(),
                           rect.height());
  buttons_->SetBoundsRect(gfx::IntersectRects(rect, buttons_bounds));
}

void ContentsSwitcherView::ButtonPressed(views::Button* sender,
                                         const ui::Event& event) {
  contents_view_->SetShowState(
      static_cast<ContentsView::ShowState>(sender->tag()));
}

}  // namespace app_list
