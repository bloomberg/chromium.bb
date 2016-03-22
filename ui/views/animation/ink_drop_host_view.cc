// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/ink_drop_host_view.h"

#include "ui/gfx/color_palette.h"
#include "ui/views/animation/ink_drop_hover.h"
#include "ui/views/animation/square_ink_drop_animation.h"

namespace views {

// Default sizes for ink drop effects.
const int kInkDropSize = 24;
const int kInkDropLargeCornerRadius = 4;

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

scoped_ptr<InkDropAnimation> InkDropHostView::CreateInkDropAnimation() const {
  gfx::Size large_drop(ink_drop_size_.width() * 4 / 3,
                       ink_drop_size_.height() * 4 / 3);

  scoped_ptr<InkDropAnimation> animation(new SquareInkDropAnimation(
      large_drop, kInkDropLargeCornerRadius, ink_drop_size_,
      kInkDropSmallCornerRadius, GetInkDropCenter(), GetInkDropBaseColor()));
  return animation;
}

scoped_ptr<InkDropHover> InkDropHostView::CreateInkDropHover() const {
  scoped_ptr<InkDropHover> hover(
      new InkDropHover(ink_drop_size_, kInkDropSmallCornerRadius,
                       GetInkDropCenter(), GetInkDropBaseColor()));
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
