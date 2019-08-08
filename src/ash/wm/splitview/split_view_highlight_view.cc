// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/splitview/split_view_highlight_view.h"

#include "ash/shell.h"
#include "ash/wm/overview/rounded_rect_view.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ui/gfx/canvas.h"
#include "ui/views/view.h"

namespace ash {

namespace {

// The amount of round applied to the corners of the highlight views.
constexpr int kHighlightScreenRoundRectRadiusDp = 4;

constexpr int kRoundRectPaddingDp = 10;

gfx::Transform CalculateTransformFromRects(const gfx::Rect& src,
                                           const gfx::Rect& dst,
                                           bool landscape) {
  // In portrait, rtl will have no effect on this view.
  const bool is_rtl = base::i18n::IsRTL() && landscape;
  const bool should_scale =
      src.width() != dst.width() || src.height() != dst.height();

  // Add a translatation. In rtl, translate in the opposite direction to account
  // for the flip.
  gfx::Transform transform;
  transform.Translate(is_rtl && !should_scale ? src.origin() - dst.origin()
                                              : dst.origin() - src.origin());
  if (should_scale) {
    // In rtl a extra translation needs to be added to account for the flipped
    // scaling.
    if (is_rtl) {
      int x_translation = 0;
      if ((src.x() > dst.x() && src.width() < dst.width()) ||
          (src.x() == dst.x() && src.width() > dst.width())) {
        x_translation = std::abs(dst.width() - src.width());
      } else {
        x_translation = -std::abs(dst.width() - src.width());
      }
      transform.Translate(gfx::Vector2d(x_translation, 0));
    }
    transform.Scale(
        static_cast<float>(dst.width()) / static_cast<float>(src.width()),
        static_cast<float>(dst.height()) / static_cast<float>(src.height()));
  }
  return transform;
}

}  // namespace

SplitViewHighlightView::SplitViewHighlightView(bool is_right_or_bottom)
    : is_right_or_bottom_(is_right_or_bottom) {
  left_top_ =
      new RoundedRectView(kHighlightScreenRoundRectRadiusDp, SK_ColorWHITE);
  right_bottom_ =
      new RoundedRectView(kHighlightScreenRoundRectRadiusDp, SK_ColorWHITE);

  left_top_->SetPaintToLayer();
  left_top_->layer()->SetFillsBoundsOpaquely(false);
  right_bottom_->SetPaintToLayer();
  right_bottom_->layer()->SetFillsBoundsOpaquely(false);

  middle_ = new views::View();
  middle_->SetPaintToLayer(ui::LAYER_SOLID_COLOR);
  middle_->layer()->SetColor(SK_ColorWHITE);

  AddChildView(left_top_);
  AddChildView(right_bottom_);
  AddChildView(middle_);
}

SplitViewHighlightView::~SplitViewHighlightView() = default;

void SplitViewHighlightView::SetBounds(
    const gfx::Rect& bounds,
    bool landscape,
    const base::Optional<SplitviewAnimationType>& animation_type) {
  if (bounds == this->bounds() && landscape == landscape_)
    return;

  landscape_ = landscape;

  const gfx::Rect old_bounds = this->bounds();
  const gfx::Vector2d offset = old_bounds.origin() - bounds.origin();
  SetBoundsRect(bounds);
  // Shift the bounds of the right bottom view if needed before applying the
  // transform.
  const bool slides_from_right = base::i18n::IsRTL() && landscape
                                     ? !is_right_or_bottom_
                                     : is_right_or_bottom_;
  if (!offset.IsZero() && animation_type &&
      (slides_from_right ||
       *animation_type == SPLITVIEW_ANIMATION_PREVIEW_AREA_NIX_INSET)) {
    gfx::Rect old_left_top_bounds = left_top_->bounds();
    gfx::Rect old_right_middle_bounds = right_bottom_->bounds();
    gfx::Rect old_middle_bounds = middle_->bounds();

    old_left_top_bounds.Offset(offset);
    old_right_middle_bounds.Offset(offset);
    old_middle_bounds.Offset(offset);

    left_top_->SetBoundsRect(old_left_top_bounds);
    right_bottom_->SetBoundsRect(old_right_middle_bounds);
    middle_->SetBoundsRect(old_middle_bounds);
  }

  // Calculate the new bounds. The middle should take as much space as possible,
  // and the other two should take just enough space so they can display rounded
  // corners.
  gfx::Rect left_top_bounds, right_bottom_bounds;
  gfx::Rect middle_bounds = bounds;

  // The thickness of the two outer views should be the amount of rounding, plus
  // a little padding. There will be some overlap to simply the code (we use a
  // rectangle that is rounded on all sides, but cover half the sides instead of
  // creating a new class that is only rounded on half the sides).
  const int thickness = kHighlightScreenRoundRectRadiusDp + kRoundRectPaddingDp;
  if (landscape) {
    left_top_bounds = gfx::Rect(0, 0, thickness, bounds.height());
    right_bottom_bounds = left_top_bounds;
    right_bottom_bounds.Offset(bounds.width() - thickness, 0);
    middle_bounds.Offset(-bounds.x(), -bounds.y());
    middle_bounds.Inset(kHighlightScreenRoundRectRadiusDp, 0);
  } else {
    left_top_bounds = gfx::Rect(0, 0, bounds.width(), thickness);
    right_bottom_bounds = left_top_bounds;
    right_bottom_bounds.Offset(0, bounds.height() - thickness);
    middle_bounds.Offset(-bounds.x(), -bounds.y());
    middle_bounds.Inset(0, kHighlightScreenRoundRectRadiusDp);
  }

  left_top_bounds = GetMirroredRect(left_top_bounds);
  right_bottom_bounds = GetMirroredRect(right_bottom_bounds);
  middle_bounds = GetMirroredRect(middle_bounds);

  // If |animation_type| has a value, calculate the needed transform from old
  // bounds to new bounds and apply it. Otherwise set the new bounds and reset
  // the transforms on all items.
  if (animation_type) {
    DoSplitviewTransformAnimation(
        middle_->layer(), *animation_type,
        CalculateTransformFromRects(middle_->bounds(), middle_bounds,
                                    landscape));
    DoSplitviewTransformAnimation(
        left_top_->layer(), *animation_type,
        CalculateTransformFromRects(left_top_->bounds(), left_top_bounds,
                                    landscape));
    DoSplitviewTransformAnimation(
        right_bottom_->layer(), *animation_type,
        CalculateTransformFromRects(right_bottom_->bounds(),
                                    right_bottom_bounds, landscape));
  } else {
    left_top_->layer()->SetTransform(gfx::Transform());
    right_bottom_->layer()->SetTransform(gfx::Transform());
    middle_->layer()->SetTransform(gfx::Transform());

    left_top_->SetBoundsRect(left_top_bounds);
    right_bottom_->SetBoundsRect(right_bottom_bounds);
    middle_->SetBoundsRect(middle_bounds);
  }
}

void SplitViewHighlightView::SetColor(SkColor color) {
  left_top_->SetBackgroundColor(color);
  right_bottom_->SetBackgroundColor(color);
  middle_->layer()->SetColor(color);
}

void SplitViewHighlightView::OnIndicatorTypeChanged(
    IndicatorState indicator_state,
    IndicatorState previous_indicator_state) {
  if (indicator_state == IndicatorState::kNone) {
    if (!SplitViewDragIndicators::IsPreviewAreaState(
            previous_indicator_state)) {
      DoSplitviewOpacityAnimation(layer(),
                                  SPLITVIEW_ANIMATION_HIGHLIGHT_FADE_OUT);
      return;
    }

    // There are two SplitViewHighlightView objects,
    // |SplitViewDragIndicatorsView::left_highlight_view_| and
    // |SplitViewDragIndicatorsView::right_highlight_view_|.
    // |was_this_the_preview| indicates that this, in the sense of the C++
    // keyword this, is the one that represented the preview area.
    const bool was_this_the_preview =
        is_right_or_bottom_ !=
        SplitViewDragIndicators::IsPreviewAreaOnLeftTopOfScreen(
            previous_indicator_state);
    if (was_this_the_preview) {
      DoSplitviewOpacityAnimation(layer(),
                                  SPLITVIEW_ANIMATION_PREVIEW_AREA_FADE_OUT);
    }
    return;
  }

  if (SplitViewDragIndicators::IsPreviewAreaState(indicator_state)) {
    // There are two SplitViewHighlightView objects,
    // |SplitViewDragIndicatorsView::left_highlight_view_| and
    // |SplitViewDragIndicatorsView::right_highlight_view_|.
    // |is_this_the_preview| indicates that this, in the sense of the C++
    // keyword this, is the one that represents the preview area.
    const bool is_this_the_preview =
        is_right_or_bottom_ !=
        SplitViewDragIndicators::IsPreviewAreaOnLeftTopOfScreen(
            indicator_state);
    DoSplitviewOpacityAnimation(
        layer(), is_this_the_preview
                     ? SPLITVIEW_ANIMATION_PREVIEW_AREA_FADE_IN
                     : SPLITVIEW_ANIMATION_OTHER_HIGHLIGHT_FADE_OUT);
    return;
  }

  if (SplitViewDragIndicators::IsPreviewAreaState(previous_indicator_state)) {
    const bool was_this_the_preview =
        is_right_or_bottom_ !=
        SplitViewDragIndicators::IsPreviewAreaOnLeftTopOfScreen(
            previous_indicator_state);
    DoSplitviewOpacityAnimation(
        layer(), Shell::Get()->split_view_controller()->InSplitViewMode()
                     ? SPLITVIEW_ANIMATION_HIGHLIGHT_FADE_OUT
                     : (was_this_the_preview
                            ? SPLITVIEW_ANIMATION_HIGHLIGHT_FADE_IN
                            : SPLITVIEW_ANIMATION_OTHER_HIGHLIGHT_FADE_IN));
    return;
  }

  // Having ruled out kNone and the "preview area" states, we know that
  // |indicator_state| is either a "drag area" state or a "cannot snap" state.
  // If there is an indicator on only one side, and if this, in the sense of the
  // C++ keyword this, is the indicator on the opposite side, then bail out.
  if (is_right_or_bottom_
          ? SplitViewDragIndicators::IsLeftIndicatorState(indicator_state)
          : SplitViewDragIndicators::IsRightIndicatorState(indicator_state)) {
    return;
  }
  SetColor(SplitViewDragIndicators::IsCannotSnapState(indicator_state)
               ? SK_ColorBLACK
               : SK_ColorWHITE);
  DoSplitviewOpacityAnimation(
      layer(), Shell::Get()->split_view_controller()->InSplitViewMode()
                   ? SPLITVIEW_ANIMATION_HIGHLIGHT_FADE_OUT
                   : SPLITVIEW_ANIMATION_HIGHLIGHT_FADE_IN);
}

}  // namespace ash
