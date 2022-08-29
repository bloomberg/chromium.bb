// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/page_switcher.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "ash/app_list/app_list_metrics.h"
#include "ash/public/cpp/app_list/app_list_color_provider.h"
#include "ash/public/cpp/pagination/pagination_model.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/icon_button.h"
#include "base/bind.h"
#include "base/i18n/number_formatting.h"
#include "base/metrics/histogram_macros.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/animation/throb_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

constexpr int kNormalButtonRadius = 3;
constexpr int kSelectedButtonRadius = 4;
constexpr SkScalar kStrokeWidth = SkIntToScalar(2);

// Constants for the button strip that grows vertically.
// The padding on top/bottom side of each button.
constexpr int kVerticalButtonPadding = 0;

// Constants for the button strip that grows horizontally.
// The padding on left/right side of each button.
constexpr int kHorizontalButtonPadding = 0;

class PageSwitcherButton : public IconButton {
 public:
  PageSwitcherButton(PressedCallback callback,
                     const std::u16string& accesible_name,
                     bool is_root_app_grid_page_switcher)
      : IconButton(std::move(callback),
                   IconButton::Type::kSmallFloating,
                   /*icon=*/nullptr,
                   accesible_name,
                   /*is_togglable=*/false,
                   /*has_border=*/false),
        is_root_app_grid_page_switcher_(is_root_app_grid_page_switcher) {
    SetFocusBehavior(views::View::FocusBehavior::ACCESSIBLE_ONLY);
  }

  PageSwitcherButton(const PageSwitcherButton&) = delete;
  PageSwitcherButton& operator=(const PageSwitcherButton&) = delete;

  ~PageSwitcherButton() override {}

  void SetSelected(bool selected) {
    if (selected == selected_)
      return;

    selected_ = selected;
    SchedulePaint();
    if (selected)
      NotifyAccessibilityEvent(ax::mojom::Event::kAlert, true);
  }

  // IconButton:
  void PaintButtonContents(gfx::Canvas* canvas) override {
    PaintButton(canvas, BuildPaintButtonInfo());
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
    info.color = AppListColorProvider::Get()->GetPageSwitcherButtonColor(
        is_root_app_grid_page_switcher_);
    if (selected_) {
      info.style = cc::PaintFlags::kFill_Style;
      info.radius = SkIntToScalar(kSelectedButtonRadius);
      info.stroke_width = SkIntToScalar(0);
    } else {
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
    flags.setStrokeWidth(info.stroke_width);
    canvas->DrawPath(path, flags);
  }

  // If this button is selected, set to true. By default, set to false;
  bool selected_ = false;

  // True if the page switcher root is the app grid.
  const bool is_root_app_grid_page_switcher_;
};

// Gets PageSwitcherButton at |index| in |buttons|.
PageSwitcherButton* GetButtonByIndex(views::View* buttons, size_t index) {
  return static_cast<PageSwitcherButton*>(buttons->children()[index]);
}

}  // namespace

PageSwitcher::PageSwitcher(PaginationModel* model,
                           bool is_root_app_grid_page_switcher,
                           bool is_tablet_mode,
                           SkColor background_color)
    : model_(model),
      buttons_(new views::View),
      is_root_app_grid_page_switcher_(is_root_app_grid_page_switcher),
      is_tablet_mode_(is_tablet_mode) {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  if (is_root_app_grid_page_switcher_) {
    buttons_->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical, gfx::Insets(),
        kVerticalButtonPadding));
  } else {
    buttons_->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
        kHorizontalButtonPadding));
  }

  AddChildView(buttons_);

  TotalPagesChanged(0, model->total_pages());
  SelectedPageChanged(-1, model->selected_page());
  model_->AddObserver(this);
}

PageSwitcher::~PageSwitcher() {
  if (model_)
    model_->RemoveObserver(this);
}

gfx::Size PageSwitcher::CalculatePreferredSize() const {
  const int max_radius = is_root_app_grid_page_switcher_
                             ? PageSwitcher::kMaxButtonRadiusForRootGrid
                             : PageSwitcher::kMaxButtonRadiusForFolderGrid;
  // Always return a size with correct width so that container resize is not
  // needed when more pages are added.
  if (is_root_app_grid_page_switcher_) {
    return gfx::Size(2 * max_radius, buttons_->GetPreferredSize().height());
  }
  return gfx::Size(buttons_->GetPreferredSize().width(), 2 * max_radius);
}

void PageSwitcher::Layout() {
  gfx::Rect rect(GetContentsBounds());
  if (rect.IsEmpty())
    return;
  gfx::Size buttons_size(buttons_->GetPreferredSize());
  rect.ClampToCenteredSize(buttons_size);
  buttons_->SetBoundsRect(rect);
}

const char* PageSwitcher::GetClassName() const {
  return "PageSwitcher";
}

void PageSwitcher::OnThemeChanged() {
  views::View::OnThemeChanged();
  if (!buttons_)
    return;
  for (auto* child : buttons_->children()) {
    if (child->GetVisible())
      child->SchedulePaint();
  }
}

void PageSwitcher::HandlePageSwitch(const ui::Event& event) {
  if (!model_ || ignore_button_press_)
    return;

  const auto& children = buttons_->children();
  const auto it = std::find(children.begin(), children.end(), event.target());
  DCHECK(it != children.end());
  const int page = std::distance(children.begin(), it);
  if (page == model_->selected_page())
    return;
  if (is_root_app_grid_page_switcher_) {
    RecordPageSwitcherSource(
        event.IsGestureEvent() ? kTouchPageIndicator : kClickPageIndicator,
        is_tablet_mode_);
  }
  model_->SelectPage(page, true /* animate */);
}

void PageSwitcher::TotalPagesChanged(int previous_page_count,
                                     int new_page_count) {
  if (!model_)
    return;

  buttons_->RemoveAllChildViews();
  for (int i = 0; i < model_->total_pages(); ++i) {
    PageSwitcherButton* button =
        buttons_->AddChildView(std::make_unique<PageSwitcherButton>(
            base::BindRepeating(&PageSwitcher::HandlePageSwitch,
                                base::Unretained(this)),
            l10n_util::GetStringFUTF16(
                IDS_APP_LIST_PAGE_SWITCHER, base::FormatNumber(i + 1),
                base::FormatNumber(model_->total_pages())),
            is_root_app_grid_page_switcher_));
    button->SetSelected(i == model_->selected_page() ? true : false);
  }
  buttons_->SetVisible(model_->total_pages() > 1);
  PreferredSizeChanged();
}

void PageSwitcher::SelectedPageChanged(int old_selected, int new_selected) {
  if (old_selected >= 0 &&
      static_cast<size_t>(old_selected) < buttons_->children().size())
    GetButtonByIndex(buttons_, static_cast<size_t>(old_selected))
        ->SetSelected(false);
  if (new_selected >= 0 &&
      static_cast<size_t>(new_selected) < buttons_->children().size())
    GetButtonByIndex(buttons_, static_cast<size_t>(new_selected))
        ->SetSelected(true);
}

}  // namespace ash
