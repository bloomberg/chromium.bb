// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/bubble/tray_bubble_view.h"

#include <algorithm>

#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/effects/SkBlurImageFilter.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/events/event.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_delegate.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/insets.h"
#include "ui/gfx/path.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace {

// Inset the arrow a bit from the edge.
const int kArrowMinOffset = 20;
const int kBubbleSpacing = 20;

}  // namespace

namespace views {

namespace internal {

// Custom border for TrayBubbleView. Contains special logic for GetBounds()
// to stack bubbles with no arrows correctly. Also calculates the arrow offset.
class TrayBubbleBorder : public views::BubbleBorder {
 public:
  TrayBubbleBorder(views::View* owner,
                   views::View* anchor,
                   TrayBubbleView::InitParams params)
      : views::BubbleBorder(params.arrow_location, params.shadow),
        owner_(owner),
        anchor_(anchor),
        tray_arrow_offset_(params.arrow_offset) {
    set_alignment(views::BubbleBorder::ALIGN_EDGE_TO_ANCHOR_EDGE);
    set_background_color(params.arrow_color);
  }

  virtual ~TrayBubbleBorder() {}

  // Overridden from views::BubbleBorder.
  // Override views::BubbleBorder to set the bubble on top of the anchor when
  // it has no arrow.
  virtual gfx::Rect GetBounds(const gfx::Rect& position_relative_to,
                              const gfx::Size& contents_size) const OVERRIDE {
    if (arrow_location() != NONE) {
      return views::BubbleBorder::GetBounds(position_relative_to,
                                            contents_size);
    }

    gfx::Size border_size(contents_size);
    gfx::Insets insets = GetInsets();
    border_size.Enlarge(insets.width(), insets.height());

    const int x = position_relative_to.x() +
        position_relative_to.width() / 2 - border_size.width() / 2;
    // Position the bubble on top of the anchor.
    const int y = position_relative_to.y() - border_size.height()
        + insets.height() - kBubbleSpacing;
    return gfx::Rect(x, y, border_size.width(), border_size.height());
  }

  void UpdateArrowOffset() {
    int arrow_offset = 0;
    if (arrow_location() == views::BubbleBorder::BOTTOM_RIGHT ||
        arrow_location() == views::BubbleBorder::BOTTOM_LEFT) {
      // Note: tray_arrow_offset_ is relative to the anchor widget.
      if (tray_arrow_offset_ ==
          TrayBubbleView::InitParams::kArrowDefaultOffset) {
        arrow_offset = kArrowMinOffset;
      } else {
        const int width = owner_->GetWidget()->GetContentsView()->width();
        gfx::Point pt(tray_arrow_offset_, 0);
        views::View::ConvertPointToScreen(
            anchor_->GetWidget()->GetRootView(), &pt);
        views::View::ConvertPointFromScreen(
            owner_->GetWidget()->GetRootView(), &pt);
        arrow_offset = pt.x();
        if (arrow_location() == views::BubbleBorder::BOTTOM_RIGHT)
          arrow_offset = width - arrow_offset;
        arrow_offset = std::max(arrow_offset, kArrowMinOffset);
      }
    } else {
      if (tray_arrow_offset_ ==
          TrayBubbleView::InitParams::kArrowDefaultOffset) {
        arrow_offset = kArrowMinOffset;
      } else {
        gfx::Point pt(0, tray_arrow_offset_);
        views::View::ConvertPointToScreen(
            anchor_->GetWidget()->GetRootView(), &pt);
        views::View::ConvertPointFromScreen(
            owner_->GetWidget()->GetRootView(), &pt);
        arrow_offset = pt.y();
        arrow_offset = std::max(arrow_offset, kArrowMinOffset);
      }
    }
    set_arrow_offset(arrow_offset);
  }

 private:
  views::View* owner_;
  views::View* anchor_;
  const int tray_arrow_offset_;

  DISALLOW_COPY_AND_ASSIGN(TrayBubbleBorder);
};

// This mask layer clips the bubble's content so that it does not overwrite the
// rounded bubble corners.
// TODO(miket): This does not work on Windows. Implement layer masking or
// alternate solutions if the TrayBubbleView is needed there in the future.
class TrayBubbleContentMask : public ui::LayerDelegate {
 public:
  explicit TrayBubbleContentMask(int corner_radius);
  virtual ~TrayBubbleContentMask();

  ui::Layer* layer() { return &layer_; }

  // Overridden from LayerDelegate.
  virtual void OnPaintLayer(gfx::Canvas* canvas) OVERRIDE;
  virtual void OnDeviceScaleFactorChanged(float device_scale_factor) OVERRIDE;
  virtual base::Closure PrepareForLayerBoundsChange() OVERRIDE;

 private:
  ui::Layer layer_;
  SkScalar corner_radius_;

  DISALLOW_COPY_AND_ASSIGN(TrayBubbleContentMask);
};

TrayBubbleContentMask::TrayBubbleContentMask(int corner_radius)
    : layer_(ui::LAYER_TEXTURED),
      corner_radius_(corner_radius) {
  layer_.set_delegate(this);
}

TrayBubbleContentMask::~TrayBubbleContentMask() {
  layer_.set_delegate(NULL);
}

void TrayBubbleContentMask::OnPaintLayer(gfx::Canvas* canvas) {
  SkPath path;
  path.addRoundRect(gfx::RectToSkRect(gfx::Rect(layer()->bounds().size())),
                    corner_radius_, corner_radius_);
  SkPaint paint;
  paint.setAlpha(255);
  paint.setStyle(SkPaint::kFill_Style);
  canvas->DrawPath(path, paint);
}

void TrayBubbleContentMask::OnDeviceScaleFactorChanged(
    float device_scale_factor) {
  // Redrawing will take care of scale factor change.
}

base::Closure TrayBubbleContentMask::PrepareForLayerBoundsChange() {
  return base::Closure();
}

// Custom layout for the bubble-view. Does the default box-layout if there is
// enough height. Otherwise, makes sure the bottom rows are visible.
class BottomAlignedBoxLayout : public views::BoxLayout {
 public:
  explicit BottomAlignedBoxLayout(TrayBubbleView* bubble_view)
      : views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 0),
        bubble_view_(bubble_view) {
  }

  virtual ~BottomAlignedBoxLayout() {}

 private:
  virtual void Layout(views::View* host) OVERRIDE {
    if (host->height() >= host->GetPreferredSize().height() ||
        !bubble_view_->is_gesture_dragging()) {
      views::BoxLayout::Layout(host);
      return;
    }

    int consumed_height = 0;
    for (int i = host->child_count() - 1;
        i >= 0 && consumed_height < host->height(); --i) {
      views::View* child = host->child_at(i);
      if (!child->visible())
        continue;
      gfx::Size size = child->GetPreferredSize();
      child->SetBounds(0, host->height() - consumed_height - size.height(),
          host->width(), size.height());
      consumed_height += size.height();
    }
  }

  TrayBubbleView* bubble_view_;

  DISALLOW_COPY_AND_ASSIGN(BottomAlignedBoxLayout);
};

}  // namespace internal

using internal::TrayBubbleBorder;
using internal::TrayBubbleContentMask;
using internal::BottomAlignedBoxLayout;

// static
const int TrayBubbleView::InitParams::kArrowDefaultOffset = -1;

TrayBubbleView::InitParams::InitParams(AnchorType anchor_type,
                                       AnchorAlignment anchor_alignment,
                                       int min_width,
                                       int max_width)
    : anchor_type(anchor_type),
      anchor_alignment(anchor_alignment),
      min_width(min_width),
      max_width(max_width),
      max_height(0),
      can_activate(false),
      close_on_deactivate(true),
      arrow_color(SK_ColorBLACK),
      arrow_location(views::BubbleBorder::NONE),
      arrow_offset(kArrowDefaultOffset),
      shadow(views::BubbleBorder::BIG_SHADOW) {
}

// static
TrayBubbleView* TrayBubbleView::Create(gfx::NativeView parent_window,
                                       views::View* anchor,
                                       Delegate* delegate,
                                       InitParams* init_params) {
  // Set arrow_location here so that it can be passed correctly to the
  // BubbleView constructor.
  if (init_params->anchor_type == ANCHOR_TYPE_TRAY) {
    if (init_params->anchor_alignment == ANCHOR_ALIGNMENT_BOTTOM) {
      init_params->arrow_location = base::i18n::IsRTL() ?
          views::BubbleBorder::BOTTOM_LEFT : views::BubbleBorder::BOTTOM_RIGHT;
    } else if (init_params->anchor_alignment == ANCHOR_ALIGNMENT_TOP) {
      init_params->arrow_location = views::BubbleBorder::TOP_LEFT;
    } else if (init_params->anchor_alignment == ANCHOR_ALIGNMENT_LEFT) {
      init_params->arrow_location = views::BubbleBorder::LEFT_BOTTOM;
    } else {
      init_params->arrow_location = views::BubbleBorder::RIGHT_BOTTOM;
    }
  } else {
    init_params->arrow_location = views::BubbleBorder::NONE;
  }

  return new TrayBubbleView(parent_window, anchor, delegate, *init_params);
}

TrayBubbleView::TrayBubbleView(gfx::NativeView parent_window,
                               views::View* anchor,
                               Delegate* delegate,
                               const InitParams& init_params)
    : views::BubbleDelegateView(anchor, init_params.arrow_location),
      params_(init_params),
      delegate_(delegate),
      preferred_width_(init_params.min_width),
      bubble_border_(NULL),
      is_gesture_dragging_(false) {
  set_parent_window(parent_window);
  set_notify_enter_exit_on_child(true);
  set_close_on_deactivate(init_params.close_on_deactivate);
  set_margins(gfx::Insets());
  SetPaintToLayer(true);
  SetFillsBoundsOpaquely(true);

  bubble_border_ = new TrayBubbleBorder(this, anchor_view(), params_);

  bubble_content_mask_.reset(
      new TrayBubbleContentMask(bubble_border_->GetBorderCornerRadius() - 1));
}

TrayBubbleView::~TrayBubbleView() {
  // Inform host items (models) that their views are being destroyed.
  if (delegate_)
    delegate_->BubbleViewDestroyed();
}

void TrayBubbleView::InitializeAndShowBubble() {
  // Must occur after call to BubbleDelegateView::CreateBubble().
  SetAlignment(views::BubbleBorder::ALIGN_EDGE_TO_ANCHOR_EDGE);
  bubble_border_->UpdateArrowOffset();

  layer()->parent()->SetMaskLayer(bubble_content_mask_->layer());

  Show();
  UpdateBubble();
}

void TrayBubbleView::UpdateBubble() {
  SizeToContents();
  bubble_content_mask_->layer()->SetBounds(layer()->bounds());
  GetWidget()->GetRootView()->SchedulePaint();
}

void TrayBubbleView::SetMaxHeight(int height) {
  params_.max_height = height;
  if (GetWidget())
    SizeToContents();
}

void TrayBubbleView::SetWidth(int width) {
  width = std::max(std::min(width, params_.max_width), params_.min_width);
  if (preferred_width_ == width)
    return;
  preferred_width_ = width;
  if (GetWidget())
    SizeToContents();
}

void TrayBubbleView::SetPaintArrow(bool paint_arrow) {
  bubble_border_->set_paint_arrow(paint_arrow);
}

gfx::Insets TrayBubbleView::GetBorderInsets() const {
  return bubble_border_->GetInsets();
}

void TrayBubbleView::Init() {
  views::BoxLayout* layout = new BottomAlignedBoxLayout(this);
  layout->set_spread_blank_space(true);
  SetLayoutManager(layout);
}

gfx::Rect TrayBubbleView::GetAnchorRect() {
  if (!delegate_)
    return gfx::Rect();
  return delegate_->GetAnchorRect(anchor_widget(),
                                  params_.anchor_type,
                                  params_.anchor_alignment);
}

bool TrayBubbleView::CanActivate() const {
  return params_.can_activate;
}

// Overridden to create BubbleFrameView and set a custom border.
views::NonClientFrameView* TrayBubbleView::CreateNonClientFrameView(
    views::Widget* widget) {
  views::BubbleFrameView* bubble_frame_view =
      new views::BubbleFrameView(margins(), bubble_border_);
  return bubble_frame_view;
}

bool TrayBubbleView::WidgetHasHitTestMask() const {
  return true;
}

void TrayBubbleView::GetWidgetHitTestMask(gfx::Path* mask) const {
  DCHECK(mask);
  mask->addRect(gfx::RectToSkRect(GetBubbleFrameView()->GetContentsBounds()));
}

gfx::Size TrayBubbleView::GetPreferredSize() {
  gfx::Size size = views::BubbleDelegateView::GetPreferredSize();
  int height = size.height();
  if (params_.max_height != 0 && height > params_.max_height)
    height = params_.max_height;
  return gfx::Size(preferred_width_, height);
}

gfx::Size TrayBubbleView::GetMaximumSize() {
  gfx::Size size = GetPreferredSize();
  size.set_width(params_.max_width);
  return size;
}

void TrayBubbleView::OnMouseEntered(const ui::MouseEvent& event) {
  if (delegate_)
    delegate_->OnMouseEnteredView();
}

void TrayBubbleView::OnMouseExited(const ui::MouseEvent& event) {
  if (delegate_)
    delegate_->OnMouseExitedView();
}

void TrayBubbleView::GetAccessibleState(ui::AccessibleViewState* state) {
  if (params_.can_activate) {
    state->role = ui::AccessibilityTypes::ROLE_WINDOW;
    state->name = delegate_->GetAccessibleNameForBubble();
  }
}

void TrayBubbleView::ChildPreferredSizeChanged(View* child) {
  SizeToContents();
}

void TrayBubbleView::ViewHierarchyChanged(bool is_add,
                                          views::View* parent,
                                          views::View* child) {
  if (is_add && child == this) {
    parent->SetPaintToLayer(true);
    parent->SetFillsBoundsOpaquely(true);
    parent->layer()->SetMasksToBounds(true);
  }
}

}  // namespace views
