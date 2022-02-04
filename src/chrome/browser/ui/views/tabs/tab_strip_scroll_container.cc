// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_strip_scroll_container.h"

#include "base/memory/raw_ptr.h"
#include "cc/paint/paint_shader.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/tabs/tab_strip_controller.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/canvas.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"

namespace {
// Define a custom FlexRule for |scroll_view_|. Equivalent to using a
// (kScaleToMinimum, kPreferred) flex specification on the tabstrip itself,
// bypassing the ScrollView.
// TODO(1132488): Make ScrollView take on TabStrip's preferred size instead.
gfx::Size TabScrollContainerFlexRule(const views::View* tab_strip,
                                     const views::View* view,
                                     const views::SizeBounds& size_bounds) {
  const gfx::Size preferred_size = tab_strip->GetPreferredSize();
  const int minimum_width = tab_strip->GetMinimumSize().width();
  const int width = std::max(
      minimum_width, size_bounds.width().min_of(preferred_size.width()));
  return gfx::Size(width, preferred_size.height());
}

std::unique_ptr<views::ImageButton> CreateScrollButton(
    views::Button::PressedCallback callback) {
  // TODO(tbergquist): These have a lot in common with the NTB and the tab
  // search buttons. Could probably extract a base class.
  auto scroll_button =
      std::make_unique<views::ImageButton>(std::move(callback));
  scroll_button->SetImageVerticalAlignment(
      views::ImageButton::VerticalAlignment::ALIGN_MIDDLE);
  scroll_button->SetImageHorizontalAlignment(
      views::ImageButton::HorizontalAlignment::ALIGN_CENTER);
  scroll_button->SetHasInkDropActionOnClick(true);
  views::InkDrop::Get(scroll_button.get())
      ->SetMode(views::InkDropHost::InkDropMode::ON);
  scroll_button->SetFocusBehavior(views::View::FocusBehavior::ACCESSIBLE_ONLY);
  scroll_button->SetPreferredSize(gfx::Size(28, 28));
  views::HighlightPathGenerator::Install(
      scroll_button.get(),
      std::make_unique<views::CircleHighlightPathGenerator>(gfx::Insets()));

  const views::FlexSpecification button_flex_spec =
      views::FlexSpecification(views::LayoutOrientation::kHorizontal,
                               views::MinimumFlexSizeRule::kScaleToMinimum,
                               views::MaximumFlexSizeRule::kPreferred);
  scroll_button->SetProperty(views::kFlexBehaviorKey, button_flex_spec);
  return scroll_button;
}

// A customized overflow indicator that paints a shadow-like gradient over the
// tabstrip.
class TabStripContainerOverflowIndicator : public views::View {
 public:
  METADATA_HEADER(TabStripContainerOverflowIndicator);
  TabStripContainerOverflowIndicator(TabStrip* tab_strip,
                                     views::OverflowIndicatorAlignment side)
      : tab_strip_(tab_strip), side_(side) {
    DCHECK(side_ == views::OverflowIndicatorAlignment::kLeft ||
           side_ == views::OverflowIndicatorAlignment::kRight);
  }

  // Making this smaller than the margin provided by the leftmost/rightmost
  // tab's tail (TabStyle::kTabOverlap / 2) makes the transition in and out of
  // the scroll state smoother.
  static constexpr int kOpaqueWidth = 8;
  // The width of the full opacity part of the shadow.
  static constexpr int kShadowSpread = 1;
  // The width of the soft edge of the shadow.
  static constexpr int kShadowBlur = 3;
  static constexpr int kTotalWidth = kOpaqueWidth + kShadowSpread + kShadowBlur;

  // views::View overrides:
  void OnPaint(gfx::Canvas* canvas) override {
    // TODO(tbergquist): Handle themes with titlebar background images.
    // TODO(tbergquist): Handle dark themes where GG800 doesn't contrast well.
    SkColor frame_color = tab_strip_->controller()->GetFrameColor(
        BrowserFrameActiveState::kUseCurrent);
    SkColor shadow_color = gfx::kGoogleGrey800;

    // Mirror how the indicator is painted for the right vs left sides.
    SkPoint points[2];
    if (side_ == views::OverflowIndicatorAlignment::kLeft) {
      points[0].iset(GetContentsBounds().origin().x(), GetContentsBounds().y());
      points[1].iset(GetContentsBounds().right(), GetContentsBounds().y());
    } else {
      points[0].iset(GetContentsBounds().right(), GetContentsBounds().y());
      points[1].iset(GetContentsBounds().origin().x(), GetContentsBounds().y());
    }

    SkColor colors[5];
    SkScalar color_positions[5];
    // Paint an opaque region on the outside.
    colors[0] = frame_color;
    colors[1] = frame_color;
    color_positions[0] = 0;
    color_positions[1] = static_cast<float>(kOpaqueWidth) / kTotalWidth;

    // Paint a shadow-like gradient on the inside.
    colors[2] = SkColorSetA(shadow_color, 0x4D);
    colors[3] = SkColorSetA(shadow_color, 0x4D);
    colors[4] = SkColorSetA(shadow_color, SK_AlphaTRANSPARENT);
    color_positions[2] = static_cast<float>(kOpaqueWidth) / kTotalWidth;
    color_positions[3] =
        static_cast<float>(kOpaqueWidth + kShadowSpread) / kTotalWidth;
    color_positions[4] = 1;

    cc::PaintFlags flags;
    flags.setShader(cc::PaintShader::MakeLinearGradient(
        points, colors, color_positions, 5, SkTileMode::kClamp));
    canvas->DrawRect(GetContentsBounds(), flags);
  }

 private:
  raw_ptr<TabStrip> tab_strip_;
  views::OverflowIndicatorAlignment side_;
};

BEGIN_METADATA(TabStripContainerOverflowIndicator, views::View)
END_METADATA

}  // namespace

TabStripScrollContainer::TabStripScrollContainer(
    std::unique_ptr<TabStrip> tab_strip)
    : tab_strip_(tab_strip.get()) {
  SetLayoutManager(std::make_unique<views::FillLayout>())
      ->SetMinimumSizeEnabled(true);

  // TODO(https://crbug.com/1132488): ScrollView doesn't propagate changes to
  // the TabStrip's preferred size; observe that manually.
  tab_strip->View::AddObserver(this);
  tab_strip->SetAvailableWidthCallback(
      base::BindRepeating(&TabStripScrollContainer::GetTabStripAvailableWidth,
                          base::Unretained(this)));

  std::unique_ptr<views::ScrollView> scroll_view =
      std::make_unique<views::ScrollView>(
          views::ScrollView::ScrollWithLayers::kEnabled);
  scroll_view_ = scroll_view.get();
  scroll_view->SetBackgroundColor(absl::nullopt);
  scroll_view->SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kHiddenButEnabled);
  scroll_view->SetTreatAllScrollEventsAsHorizontal(true);
  scroll_view->SetContents(std::move(tab_strip));

  scroll_view->SetDrawOverflowIndicator(true);
  left_overflow_indicator_ = scroll_view->SetCustomOverflowIndicator(
      views::OverflowIndicatorAlignment::kLeft,
      std::make_unique<TabStripContainerOverflowIndicator>(
          tab_strip_, views::OverflowIndicatorAlignment::kLeft),
      TabStripContainerOverflowIndicator::kTotalWidth, false);
  right_overflow_indicator_ = scroll_view->SetCustomOverflowIndicator(
      views::OverflowIndicatorAlignment::kRight,
      std::make_unique<TabStripContainerOverflowIndicator>(
          tab_strip_, views::OverflowIndicatorAlignment::kRight),
      TabStripContainerOverflowIndicator::kTotalWidth, false);

  // This base::Unretained is safe because the callback is called by the
  // layout manager, which is cleaned up before view children like
  // |scroll_view| (which owns |tab_strip_|).
  scroll_view->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(base::BindRepeating(
          &TabScrollContainerFlexRule, base::Unretained(tab_strip_))));

  std::unique_ptr<views::View> scroll_button_container =
      std::make_unique<views::View>();
  views::FlexLayout* scroll_button_layout =
      scroll_button_container->SetLayoutManager(
          std::make_unique<views::FlexLayout>());
  scroll_button_layout->SetOrientation(views::LayoutOrientation::kHorizontal);

  leading_scroll_button_ =
      scroll_button_container->AddChildView(CreateScrollButton(
          base::BindRepeating(&TabStripScrollContainer::ScrollTowardsLeadingTab,
                              base::Unretained(this))));
  leading_scroll_button_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_ACCNAME_TAB_SCROLL_LEADING));
  trailing_scroll_button_ = scroll_button_container->AddChildView(
      CreateScrollButton(base::BindRepeating(
          &TabStripScrollContainer::ScrollTowardsTrailingTab,
          base::Unretained(this))));
  trailing_scroll_button_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_ACCNAME_TAB_SCROLL_TRAILING));

  // The space in dips between the scroll buttons and the NTB.
  constexpr int kScrollButtonsTrailingMargin = 8;
  trailing_scroll_button_->SetProperty(
      views::kMarginsKey, gfx::Insets(0, 0, 0, kScrollButtonsTrailingMargin));

  // The default layout orientation (kHorizontal) and cross axis alignment
  // (kStretch) work for our use case.
  overflow_view_ = AddChildView(std::make_unique<OverflowView>(
      std::move(scroll_view), std::move(scroll_button_container)));
}

TabStripScrollContainer::~TabStripScrollContainer() = default;

void TabStripScrollContainer::OnViewPreferredSizeChanged(views::View* view) {
  DCHECK_EQ(tab_strip_, view);

  PreferredSizeChanged();
}

int TabStripScrollContainer::GetTabStripAvailableWidth() const {
  return overflow_view_->GetAvailableSize(scroll_view_).width().value();
}

void TabStripScrollContainer::ScrollTowardsLeadingTab() {
  gfx::Rect visible_content = scroll_view_->GetVisibleRect();
  gfx::Rect scroll(visible_content.x() - visible_content.width(),
                   visible_content.y(), visible_content.width(),
                   visible_content.height());
  scroll_view_->contents()->ScrollRectToVisible(scroll);
}

void TabStripScrollContainer::ScrollTowardsTrailingTab() {
  gfx::Rect visible_content = scroll_view_->GetVisibleRect();
  gfx::Rect scroll(visible_content.x() + visible_content.width(),
                   visible_content.y(), visible_content.width(),
                   visible_content.height());
  scroll_view_->contents()->ScrollRectToVisible(scroll);
}

void TabStripScrollContainer::FrameColorsChanged() {
  const SkColor background_color = tab_strip_->GetTabBackgroundColor(
      TabActive::kInactive, BrowserFrameActiveState::kUseCurrent);
  SkColor foreground_color =
      tab_strip_->GetTabForegroundColor(TabActive::kInactive, background_color);
  views::SetImageFromVectorIconWithColor(
      leading_scroll_button_, kScrollingTabstripLeadingIcon, foreground_color);
  views::SetImageFromVectorIconWithColor(trailing_scroll_button_,
                                         kScrollingTabstripTrailingIcon,
                                         foreground_color);
  left_overflow_indicator_->SchedulePaint();
  right_overflow_indicator_->SchedulePaint();
}

void TabStripScrollContainer::OnThemeChanged() {
  View::OnThemeChanged();
  FrameColorsChanged();
}

BEGIN_METADATA(TabStripScrollContainer, views::View)
ADD_READONLY_PROPERTY_METADATA(int, TabStripAvailableWidth)
END_METADATA
