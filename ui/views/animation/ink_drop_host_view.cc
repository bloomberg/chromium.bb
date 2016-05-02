// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/ink_drop_host_view.h"

#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/views/animation/ink_drop_hover.h"
#include "ui/views/animation/square_ink_drop_ripple.h"

namespace views {

// Default sizes for ink drop effects.
const int kInkDropSize = 24;
const int kInkDropLargeCornerRadius = 4;

// The scale factor to compute the large ink drop size.
const float kLargeInkDropScale = 1.333f;

namespace {

gfx::Size CalculateLargeInkDropSize(const gfx::Size small_size) {
  return gfx::ScaleToCeiledSize(gfx::Size(small_size), kLargeInkDropScale);
}

}  // namespace

// static
const int InkDropHostView::kInkDropSmallCornerRadius = 2;

InkDropHostView::InkDropHostView()
    : ink_drop_size_(kInkDropSize, kInkDropSize) {}

InkDropHostView::~InkDropHostView() {}

void InkDropHostView::AddInkDropLayer(ui::Layer* ink_drop_layer) {
  SetPaintToLayer(true);
  layer()->SetFillsBoundsOpaquely(false);
  layer()->Add(ink_drop_layer);
  layer()->StackAtBottom(ink_drop_layer);
}

void InkDropHostView::RemoveInkDropLayer(ui::Layer* ink_drop_layer) {
  layer()->Remove(ink_drop_layer);
  SetPaintToLayer(false);
}

std::unique_ptr<InkDropRipple> InkDropHostView::CreateInkDropRipple() const {
  std::unique_ptr<InkDropRipple> ripple(new SquareInkDropRipple(
      CalculateLargeInkDropSize(ink_drop_size_), kInkDropLargeCornerRadius,
      ink_drop_size_, kInkDropSmallCornerRadius, GetInkDropCenter(),
      GetInkDropBaseColor()));
  return ripple;
}

std::unique_ptr<InkDropHover> InkDropHostView::CreateInkDropHover() const {
  std::unique_ptr<InkDropHover> hover(
      new InkDropHover(ink_drop_size_, kInkDropSmallCornerRadius,
                       GetInkDropCenter(), GetInkDropBaseColor()));
  hover->set_explode_size(CalculateLargeInkDropSize(ink_drop_size_));
  return hover;
}

gfx::Point InkDropHostView::GetInkDropCenter() const {
  return GetLocalBounds().CenterPoint();
}

SkColor InkDropHostView::GetInkDropBaseColor() const {
  NOTREACHED();
  return gfx::kPlaceholderColor;
}

}  // namespace views
