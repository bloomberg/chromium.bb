// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/page_switcher_vertical.h"

#include <algorithm>

#include "base/macros.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/pagination_model.h"
#include "ui/gfx/animation/throb_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/controls/button/custom_button.h"
#include "ui/views/layout/box_layout.h"

namespace app_list {

namespace {

const int kPreferredWidth = 58;

const int kMaxButtonSpacing = 18;
const int kMinButtonSpacing = 4;
const int kMaxButtonHeight = 68;
const int kMinButtonHeight = 28;
const int kButtonWidth = 6;
const int kButtonCornerRadius = 2;
const int kButtonStripPadding = 20;

class PageSwitcherButton : public views::CustomButton {
 public:
  explicit PageSwitcherButton(views::ButtonListener* listener)
      : views::CustomButton(listener),
        button_height_(kMaxButtonHeight),
        selected_range_(0) {}
  ~PageSwitcherButton() override {}

  void SetSelectedRange(double selected_range) {
    if (selected_range_ == selected_range)
      return;

    selected_range_ = selected_range;
    SchedulePaint();
  }

  void set_button_height(int button_height) { button_height_ = button_height; }

  // Overridden from views::View:
  gfx::Size CalculatePreferredSize() const override {
    return gfx::Size(kButtonWidth, button_height_);
  }

  void PaintButtonContents(gfx::Canvas* canvas) override {
    if (state() == STATE_HOVERED)
      PaintButton(canvas, kPagerHoverColor);
    else
      PaintButton(canvas, kPagerNormalColor);
  }

  void OnGestureEvent(ui::GestureEvent* event) override {
    CustomButton::OnGestureEvent(event);

    if (event->type() == ui::ET_GESTURE_TAP_DOWN)
      SetState(views::CustomButton::STATE_HOVERED);
    else if (event->type() == ui::ET_GESTURE_TAP_CANCEL ||
             event->type() == ui::ET_GESTURE_TAP)
      SetState(views::CustomButton::STATE_NORMAL);
    SchedulePaint();
  }

 private:
  // Paints a button that has two rounded corner at bottom.
  void PaintButton(gfx::Canvas* canvas, SkColor base_color) {
    gfx::Rect rect(GetContentsBounds());
    rect.ClampToCenteredSize(gfx::Size(kButtonWidth, button_height_));

    SkPath path;
    path.addRoundRect(gfx::RectToSkRect(rect),
                      SkIntToScalar(kButtonCornerRadius),
                      SkIntToScalar(kButtonCornerRadius));

    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setColor(base_color);
    canvas->DrawPath(path, flags);

    int selected_start_y = 0;
    int selected_height = 0;
    if (selected_range_ > 0) {
      selected_height = selected_range_ * rect.height();
    } else if (selected_range_ < 0) {
      selected_height = -selected_range_ * rect.height();
      selected_start_y = rect.bottom() - selected_height;
    }

    if (selected_height) {
      gfx::Rect selected_rect(rect);
      selected_rect.set_y(selected_start_y);
      selected_rect.set_height(selected_height);

      SkPath selected_path;
      selected_path.addRoundRect(gfx::RectToSkRect(selected_rect),
                                 SkIntToScalar(kButtonCornerRadius),
                                 SkIntToScalar(kButtonCornerRadius));
      flags.setColor(kPagerSelectedColor);
      canvas->DrawPath(selected_path, flags);
    }
  }

  int button_height_;

  // [-1, 1] range that represents the portion of the button that should be
  // painted with kSelectedColor. Positive range starts from top side and
  // negative range starts from the bottom side.
  double selected_range_;

  DISALLOW_COPY_AND_ASSIGN(PageSwitcherButton);
};

// Gets PageSwitcherButton at |index| in |buttons|.
PageSwitcherButton* GetButtonByIndex(views::View* buttons, int index) {
  return static_cast<PageSwitcherButton*>(buttons->child_at(index));
}

}  // namespace

PageSwitcherVertical::PageSwitcherVertical(PaginationModel* model)
    : model_(model), buttons_(new views::View) {
  AddChildView(buttons_);

  TotalPagesChanged();
  SelectedPageChanged(-1, model->selected_page());
  model_->AddObserver(this);
}

PageSwitcherVertical::~PageSwitcherVertical() {
  model_->RemoveObserver(this);
}

int PageSwitcherVertical::GetPageForPoint(const gfx::Point& point) const {
  if (!buttons_->bounds().Contains(point))
    return -1;

  gfx::Point buttons_point(point);
  views::View::ConvertPointToTarget(this, buttons_, &buttons_point);

  for (int i = 0; i < buttons_->child_count(); ++i) {
    const views::View* button = buttons_->child_at(i);
    if (button->bounds().Contains(buttons_point))
      return i;
  }

  return -1;
}

void PageSwitcherVertical::UpdateUIForDragPoint(const gfx::Point& point) {
  int page = GetPageForPoint(point);

  const int button_count = buttons_->child_count();
  if (page >= 0 && page < button_count) {
    PageSwitcherButton* button =
        static_cast<PageSwitcherButton*>(buttons_->child_at(page));
    button->SetState(views::CustomButton::STATE_HOVERED);
    return;
  }

  for (int i = 0; i < button_count; ++i) {
    PageSwitcherButton* button =
        static_cast<PageSwitcherButton*>(buttons_->child_at(i));
    button->SetState(views::CustomButton::STATE_NORMAL);
  }
}

gfx::Size PageSwitcherVertical::CalculatePreferredSize() const {
  // Always return a size with correct width so that container resize is not
  // needed when more pages are added.
  return gfx::Size(kPreferredWidth, buttons_->GetPreferredSize().height());
}

void PageSwitcherVertical::Layout() {
  gfx::Rect rect(GetContentsBounds());

  CalculateButtonHeightAndSpacing(rect.height());

  // Makes |buttons_| vertically center and horizontally fill.
  gfx::Size buttons_size(buttons_->GetPreferredSize());
  gfx::Rect buttons_bounds(rect.x(),
                           rect.CenterPoint().y() - buttons_size.height() / 2,
                           rect.width(), buttons_size.height());
  buttons_->SetBoundsRect(gfx::IntersectRects(rect, buttons_bounds));
}

void PageSwitcherVertical::CalculateButtonHeightAndSpacing(
    int contents_height) {
  const int button_count = buttons_->child_count();
  if (!button_count)
    return;

  contents_height -= 2 * kButtonStripPadding;

  int button_height = kMinButtonHeight;
  int button_spacing = kMinButtonSpacing;
  if (button_count > 1) {
    button_spacing =
        (contents_height - button_height * button_count) / (button_count - 1);
    button_spacing = std::min(kMaxButtonSpacing,
                              std::max(kMinButtonSpacing, button_spacing));
  }

  button_height =
      (contents_height - (button_count - 1) * button_spacing) / button_count;
  button_height =
      std::min(kMaxButtonHeight, std::max(kMinButtonHeight, button_height));

  buttons_->SetLayoutManager(new views::BoxLayout(
      views::BoxLayout::kVertical, gfx::Insets(kButtonStripPadding, 0),
      button_spacing));
  for (int i = 0; i < button_count; ++i) {
    PageSwitcherButton* button =
        static_cast<PageSwitcherButton*>(buttons_->child_at(i));
    button->set_button_height(button_height);
  }
}

void PageSwitcherVertical::ButtonPressed(views::Button* sender,
                                         const ui::Event& event) {
  for (int i = 0; i < buttons_->child_count(); ++i) {
    if (sender == static_cast<views::Button*>(buttons_->child_at(i))) {
      model_->SelectPage(i, true /* animate */);
      break;
    }
  }
}

void PageSwitcherVertical::TotalPagesChanged() {
  buttons_->RemoveAllChildViews(true);
  for (int i = 0; i < model_->total_pages(); ++i) {
    PageSwitcherButton* button = new PageSwitcherButton(this);
    button->SetSelectedRange(i == model_->selected_page() ? 1 : 0);
    buttons_->AddChildView(button);
  }
  buttons_->SetVisible(model_->total_pages() > 1);
  Layout();
}

void PageSwitcherVertical::SelectedPageChanged(int old_selected,
                                               int new_selected) {
  if (old_selected >= 0 && old_selected < buttons_->child_count())
    GetButtonByIndex(buttons_, old_selected)->SetSelectedRange(0);
  if (new_selected >= 0 && new_selected < buttons_->child_count())
    GetButtonByIndex(buttons_, new_selected)->SetSelectedRange(1);
}

void PageSwitcherVertical::TransitionStarted() {}

void PageSwitcherVertical::TransitionChanged() {
  const int current_page = model_->selected_page();
  const int target_page = model_->transition().target_page;

  double progress = model_->transition().progress;
  double remaining = progress - 1;

  if (current_page > target_page) {
    remaining = -remaining;
    progress = -progress;
  }

  GetButtonByIndex(buttons_, current_page)->SetSelectedRange(remaining);
  if (model_->is_valid_page(target_page))
    GetButtonByIndex(buttons_, target_page)->SetSelectedRange(progress);
}

}  // namespace app_list
