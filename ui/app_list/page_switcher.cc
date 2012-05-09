// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/page_switcher.h"

#include "third_party/skia/include/core/SkPath.h"
#include "ui/app_list/pagination_model.h"
#include "ui/base/animation/throb_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/views/controls/button/custom_button.h"
#include "ui/views/layout/box_layout.h"

namespace {

const int kButtonSpacing = 10;
const int kButtonWidth = 60;
const int kButtonHeight = 6;
const int kButtonHeightPadding = 10;
const int kButtonCornerRadius = 2;

// 0.16 black
const SkColor kHoverColor = SkColorSetARGB(0x2A, 0x00, 0x00, 0x00);

// 0.2 black
const SkColor kNormalColor = SkColorSetARGB(0x33, 0x00, 0x00, 0x00);

// 0.33 black
const SkColor kSelectedColor = SkColorSetARGB(0x55, 0x00, 0x00, 0x00);

class PageSwitcherButton : public views::CustomButton {
 public:
  explicit PageSwitcherButton(views::ButtonListener* listener)
      : views::CustomButton(listener),
        selected_(false) {
  }
  virtual ~PageSwitcherButton() {}

  void SetSelected(bool selected) {
    if (selected == selected_)
      return;

    selected_ = selected;
    SchedulePaint();
  }

  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize() OVERRIDE {
    return gfx::Size(kButtonWidth, kButtonHeight + kButtonHeightPadding);
  }

  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {
    if (selected_ || state() == BS_PUSHED) {
      PaintButton(canvas, kSelectedColor);
    } else if (state() == BS_HOT) {
      PaintButton(canvas, kHoverColor);
    } else {
      PaintButton(canvas, kNormalColor);
    }
  }

 private:
  // Paints a button that has two rounded corner at bottom.
  void PaintButton(gfx::Canvas* canvas, SkColor color) {
    gfx::Rect rect(GetContentsBounds());
    rect.set_height(kButtonHeight);

    gfx::Point center = rect.CenterPoint();

    SkPath path;
    path.incReserve(12);
    path.moveTo(SkIntToScalar(rect.x()), SkIntToScalar(rect.y()));
    path.arcTo(SkIntToScalar(rect.x()), SkIntToScalar(rect.bottom()),
               SkIntToScalar(center.x()), SkIntToScalar(rect.bottom()),
               SkIntToScalar(kButtonCornerRadius));
    path.arcTo(SkIntToScalar(rect.right()), SkIntToScalar(rect.bottom()),
               SkIntToScalar(rect.right()), SkIntToScalar(rect.y()),
               SkIntToScalar(kButtonCornerRadius));
    path.lineTo(SkIntToScalar(rect.right()), SkIntToScalar(rect.y()));
    path.close();

    SkPaint paint;
    paint.setStyle(SkPaint::kFill_Style);
    paint.setColor(color);
    canvas->DrawPath(path, paint);
  }

  bool selected_;

  DISALLOW_COPY_AND_ASSIGN(PageSwitcherButton);
};

// Gets PageSwitcherButton at |index| in |buttons|.
PageSwitcherButton* GetButtonByIndex(views::View* buttons, int index) {
  return static_cast<PageSwitcherButton*>(buttons->child_at(index));
}

}  // namespace

namespace app_list {

PageSwitcher::PageSwitcher(PaginationModel* model)
    : model_(model),
      buttons_(NULL) {
  buttons_ = new views::View;
  buttons_->SetLayoutManager(new views::BoxLayout(
      views::BoxLayout::kHorizontal, 0, 0, kButtonSpacing));
  AddChildView(buttons_);

  TotalPagesChanged();
  SelectedPageChanged(-1, model->selected_page());
  model_->AddObserver(this);
}

PageSwitcher::~PageSwitcher() {
  model_->RemoveObserver(this);
}

gfx::Size PageSwitcher::GetPreferredSize() {
  // Always return a size with correct height so that container resize is not
  // needed when more pages are added.
  return gfx::Size(kButtonWidth, kButtonHeight + kButtonHeightPadding);
}

void PageSwitcher::Layout() {
  gfx::Rect rect(GetContentsBounds());
  buttons_->SetBoundsRect(rect.Center(buttons_->GetPreferredSize()));
}

void PageSwitcher::ButtonPressed(views::Button* sender,
                                 const views::Event& event) {
  for (int i = 0; i < buttons_->child_count(); ++i) {
    if (sender == static_cast<views::Button*>(buttons_->child_at(i))) {
      model_->SelectPage(i);
      break;
    }
  }
}

void PageSwitcher::TotalPagesChanged() {
  buttons_->RemoveAllChildViews(true);
  for (int i = 0; i < model_->total_pages(); ++i) {
    PageSwitcherButton* button = new PageSwitcherButton(this);
    button->SetSelected(i == model_->selected_page());
    buttons_->AddChildView(button);
  }
  buttons_->SetVisible(model_->total_pages() > 1);
  Layout();
}

void PageSwitcher::SelectedPageChanged(int old_selected, int new_selected) {
  if (old_selected >= 0 && old_selected < buttons_->child_count())
    GetButtonByIndex(buttons_, old_selected)->SetSelected(false);
  if (new_selected >= 0 && new_selected < buttons_->child_count())
    GetButtonByIndex(buttons_, new_selected)->SetSelected(true);
}

}  // namespace app_list
