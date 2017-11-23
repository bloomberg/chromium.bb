// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/page_switcher_vertical.h"

#include <algorithm>

#include "base/i18n/number_formatting.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/pagination_model.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/animation/throb_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/skia_util.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_mask.h"
#include "ui/views/animation/ink_drop_painted_layer_delegates.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/layout/box_layout.h"

namespace app_list {

namespace {

constexpr int kNormalButtonRadius = 3;
constexpr int kSelectedButtonRadius = 4;
constexpr int kInkDropRadius = 8;
// The padding on top/bottom side of each button.
constexpr int kButtonPadding = 12;
constexpr int kMaxButtonRadius = 8;
constexpr int kPreferredButtonStripWidth = kMaxButtonRadius * 2;

// The selected button color.
constexpr SkColor kSelectedButtonColor = SK_ColorWHITE;
// The normal button color (54% white).
constexpr SkColor kNormalColor = SkColorSetA(SK_ColorWHITE, 138);
constexpr SkColor kInkDropBaseColor = SK_ColorWHITE;
constexpr SkColor kInkDropRippleColor = SkColorSetA(kInkDropBaseColor, 20);
constexpr SkColor kInkDropHighlightColor = SkColorSetA(kInkDropBaseColor, 15);

constexpr SkScalar kStrokeWidth = SkIntToScalar(1);

class PageSwitcherButton : public views::Button {
 public:
  explicit PageSwitcherButton(views::ButtonListener* listener)
      : views::Button(listener) {
    SetInkDropMode(InkDropMode::ON);
  }

  ~PageSwitcherButton() override {}

  void SetSelected(bool selected) {
    if (selected == selected_)
      return;

    selected_ = selected;
    SchedulePaint();
    if (selected)
      NotifyAccessibilityEvent(ui::AX_EVENT_ALERT, true);
  }

  // Overridden from views::View:
  gfx::Size CalculatePreferredSize() const override {
    return gfx::Size(kMaxButtonRadius * 2, kMaxButtonRadius * 2);
  }

  void PaintButtonContents(gfx::Canvas* canvas) override {
    PaintButton(canvas, BuildPaintButtonInfo());
  }

 protected:
  std::unique_ptr<views::InkDrop> CreateInkDrop() override {
    std::unique_ptr<views::InkDropImpl> ink_drop =
        Button::CreateDefaultInkDropImpl();
    ink_drop->SetShowHighlightOnHover(false);
    ink_drop->SetAutoHighlightMode(
        views::InkDropImpl::AutoHighlightMode::SHOW_ON_RIPPLE);
    return std::move(ink_drop);
  }

  std::unique_ptr<views::InkDropMask> CreateInkDropMask() const override {
    return std::make_unique<views::CircleInkDropMask>(
        size(), GetLocalBounds().CenterPoint(), kMaxButtonRadius);
  }

  std::unique_ptr<views::InkDropRipple> CreateInkDropRipple() const override {
    gfx::Point center = GetLocalBounds().CenterPoint();
    gfx::Rect bounds(center.x() - kMaxButtonRadius,
                     center.y() - kMaxButtonRadius, 2 * kMaxButtonRadius,
                     2 * kMaxButtonRadius);
    return std::make_unique<views::FloodFillInkDropRipple>(
        size(), GetLocalBounds().InsetsFrom(bounds),
        GetInkDropCenterBasedOnLastEvent(), kInkDropRippleColor, 1.0f);
  }

  std::unique_ptr<views::InkDropHighlight> CreateInkDropHighlight()
      const override {
    return std::make_unique<views::InkDropHighlight>(
        gfx::PointF(GetLocalBounds().CenterPoint()),
        std::make_unique<views::CircleLayerDelegate>(kInkDropHighlightColor,
                                                     kInkDropRadius));
  }

  void NotifyClick(const ui::Event& event) override {
    Button::NotifyClick(event);
    GetInkDrop()->AnimateToState(views::InkDropState::ACTION_TRIGGERED);
  }

 private:
  // Stores the information of how to paint the button.
  struct PaintButtonInfo {
    SkColor color;
    cc::PaintFlags::Style style;
    SkScalar radius;
    SkScalar stroke_width;
  };

  // Returns the information of how to paint selected/normal button.
  PaintButtonInfo BuildPaintButtonInfo() {
    PaintButtonInfo info;
    if (selected_) {
      info.color = kSelectedButtonColor;
      info.style = cc::PaintFlags::kFill_Style;
      info.radius = SkIntToScalar(kSelectedButtonRadius);
      info.stroke_width = SkIntToScalar(0);
    } else {
      info.color = kNormalColor;
      info.style = cc::PaintFlags::kStroke_Style;
      info.radius = SkIntToScalar(kNormalButtonRadius);
      info.stroke_width = kStrokeWidth;
    }
    return info;
  }

  // Paints a button based on the |info|.
  void PaintButton(gfx::Canvas* canvas, PaintButtonInfo info) {
    gfx::Rect rect(GetContentsBounds());
    SkPath path;
    path.addCircle(rect.CenterPoint().x(), rect.CenterPoint().y(), info.radius);

    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setStyle(info.style);
    flags.setColor(info.color);
    canvas->DrawPath(path, flags);
  }

  // If this button is selected, set to true. By default, set to false;
  bool selected_ = false;

  DISALLOW_COPY_AND_ASSIGN(PageSwitcherButton);
};

// Gets PageSwitcherButton at |index| in |buttons|.
PageSwitcherButton* GetButtonByIndex(views::View* buttons, int index) {
  return static_cast<PageSwitcherButton*>(buttons->child_at(index));
}

}  // namespace

PageSwitcherVertical::PageSwitcherVertical(PaginationModel* model)
    : model_(model), buttons_(new views::View) {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  buttons_->SetLayoutManager(new views::BoxLayout(
      views::BoxLayout::kVertical, gfx::Insets(), kButtonPadding));

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
    button->SetState(views::Button::STATE_HOVERED);
    return;
  }

  for (int i = 0; i < button_count; ++i) {
    PageSwitcherButton* button =
        static_cast<PageSwitcherButton*>(buttons_->child_at(i));
    button->SetState(views::Button::STATE_NORMAL);
  }
}

gfx::Rect PageSwitcherVertical::GetButtonsBoundsInScreen() {
  return buttons_->GetBoundsInScreen();
}

gfx::Size PageSwitcherVertical::CalculatePreferredSize() const {
  // Always return a size with correct width so that container resize is not
  // needed when more pages are added.
  return gfx::Size(kPreferredButtonStripWidth,
                   buttons_->GetPreferredSize().height());
}

void PageSwitcherVertical::Layout() {
  gfx::Rect rect(GetContentsBounds());

  // Makes |buttons_| vertically center and horizontally fill.
  gfx::Size buttons_size(buttons_->GetPreferredSize());
  gfx::Rect buttons_bounds(rect.x(),
                           rect.CenterPoint().y() - buttons_size.height() / 2,
                           rect.width(), buttons_size.height());
  buttons_->SetBoundsRect(gfx::IntersectRects(rect, buttons_bounds));
}

void PageSwitcherVertical::ButtonPressed(views::Button* sender,
                                         const ui::Event& event) {
  for (int i = 0; i < buttons_->child_count(); ++i) {
    if (sender == static_cast<views::Button*>(buttons_->child_at(i))) {
      if (model_->selected_page() == i)
        break;
      UMA_HISTOGRAM_ENUMERATION(
          kAppListPageSwitcherSourceHistogram,
          event.IsGestureEvent() ? kTouchPageIndicator : kClickPageIndicator,
          kMaxAppListPageSwitcherSource);
      model_->SelectPage(i, true /* animate */);
      break;
    }
  }
}

void PageSwitcherVertical::TotalPagesChanged() {
  buttons_->RemoveAllChildViews(true);
  for (int i = 0; i < model_->total_pages(); ++i) {
    PageSwitcherButton* button = new PageSwitcherButton(this);
    button->SetAccessibleName(l10n_util::GetStringFUTF16(
        IDS_APP_LIST_PAGE_SWITCHER, base::FormatNumber(i + 1),
        base::FormatNumber(model_->total_pages())));
    button->SetSelected(i == model_->selected_page() ? true : false);
    buttons_->AddChildView(button);
  }
  buttons_->SetVisible(model_->total_pages() > 1);
  Layout();
}

void PageSwitcherVertical::SelectedPageChanged(int old_selected,
                                               int new_selected) {
  if (old_selected >= 0 && old_selected < buttons_->child_count())
    GetButtonByIndex(buttons_, old_selected)->SetSelected(false);
  if (new_selected >= 0 && new_selected < buttons_->child_count())
    GetButtonByIndex(buttons_, new_selected)->SetSelected(true);
}

void PageSwitcherVertical::TransitionStarted() {}

void PageSwitcherVertical::TransitionChanged() {}

void PageSwitcherVertical::TransitionEnded() {}

}  // namespace app_list
