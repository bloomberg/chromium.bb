// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/fade_animation.h"

#include <algorithm>
#include <memory>

#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/layer_delegate.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/layout_manager_base.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"
#include "ui/views/style/typography_provider.h"

namespace views {
namespace examples {

class CenteringLayoutManager : public LayoutManagerBase {
 public:
  CenteringLayoutManager() = default;
  ~CenteringLayoutManager() override = default;

 protected:
  // LayoutManagerBase:
  ProposedLayout CalculateProposedLayout(
      const SizeBounds& size_bounds) const override;
};

constexpr gfx::Size FadingView::kSize;

FadingView::FadingView() {
  Builder<FadingView>(this)
      .SetUseDefaultFillLayout(true)
      .SetPreferredSize(kSize)
      .AddChildren(
          Builder<BoxLayoutView>()
              .CopyAddressTo(&primary_view_)
              .SetBorder(CreateRoundedRectBorder(1, kCornerRadius,
                                                 gfx::kGoogleGrey900))
              .SetBackground(CreateRoundedRectBackground(SK_ColorWHITE, 12.0f))
              .SetPaintToLayer()
              .SetOrientation(BoxLayout::Orientation::kVertical)
              .SetMainAxisAlignment(BoxLayout::MainAxisAlignment::kCenter)
              .SetBetweenChildSpacing(kSpacing)
              .AddChildren(Builder<Label>()
                               .SetText(u"Primary Title")
                               .SetTextContext(style::CONTEXT_DIALOG_TITLE)
                               .SetTextStyle(style::STYLE_PRIMARY)
                               .SetVerticalAlignment(gfx::ALIGN_MIDDLE),
                           Builder<Label>()
                               .SetText(u"Secondary Title")
                               .SetTextContext(style::CONTEXT_LABEL)
                               .SetTextStyle(style::STYLE_SECONDARY)
                               .SetVerticalAlignment(gfx::ALIGN_MIDDLE)),
          Builder<BoxLayoutView>()
              .CopyAddressTo(&secondary_view_)
              .SetBorder(CreateRoundedRectBorder(1, kCornerRadius,
                                                 gfx::kGoogleGrey900))
              .SetBackground(CreateRoundedRectBackground(SK_ColorWHITE, 12.0f))
              .SetPaintToLayer()
              .SetOrientation(BoxLayout::Orientation::kVertical)
              .SetMainAxisAlignment(BoxLayout::MainAxisAlignment::kCenter)
              .SetBetweenChildSpacing(kSpacing)
              .AddChild(Builder<Label>()
                            .SetText(u"Working...")
                            .SetTextContext(style::CONTEXT_DIALOG_TITLE)
                            .SetTextStyle(style::STYLE_PRIMARY)
                            .SetVerticalAlignment(gfx::ALIGN_MIDDLE)
                            .SetEnabledColor(gfx::kGoogleBlue800)))
      .BuildChildren();
  primary_view_->layer()->SetRoundedCornerRadius(
      gfx::RoundedCornersF(kCornerRadiusF));
  secondary_view_->layer()->SetRoundedCornerRadius(
      gfx::RoundedCornersF(kCornerRadiusF));
  secondary_view_->layer()->SetOpacity(0.0f);

  AnimationBuilder()
      .Repeatedly()
      .Offset(base::Seconds(2))
      .SetDuration(base::Seconds(1))
      .SetOpacity(primary_view_, 0.0f)
      .SetOpacity(secondary_view_, 1.0f)
      .Offset(base::Seconds(2))
      .SetDuration(base::Seconds(1))
      .SetOpacity(primary_view_, 1.0f)
      .SetOpacity(secondary_view_, 0.0f);
}

FadingView::~FadingView() = default;

ProposedLayout CenteringLayoutManager::CalculateProposedLayout(
    const SizeBounds& size_bounds) const {
  ProposedLayout layout;
  const auto& children = host_view()->children();

  gfx::Rect host_bounds(size_bounds.width().min_of(host_view()->width()),
                        size_bounds.height().min_of(host_view()->height()));
  for (auto* child : children) {
    gfx::Size preferred_size = child->GetPreferredSize();
    gfx::Rect child_bounds = host_bounds;
    child_bounds.ClampToCenteredSize(preferred_size);

    layout.child_layouts.push_back(
        {child, true, child_bounds, SizeBounds(preferred_size)});
  }
  layout.host_size = host_bounds.size();
  return layout;
}

FadeAnimationExample::FadeAnimationExample() : ExampleBase("Fade Animation") {}

FadeAnimationExample::~FadeAnimationExample() = default;

void FadeAnimationExample::CreateExampleView(View* container) {
  container->SetBackground(CreateSolidBackground(SK_ColorWHITE));
  container->SetLayoutManager(std::make_unique<CenteringLayoutManager>());
  container->AddChildView(std::make_unique<FadingView>());
}

}  // namespace examples
}  // namespace views
