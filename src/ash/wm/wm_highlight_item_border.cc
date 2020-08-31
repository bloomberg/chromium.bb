// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/wm_highlight_item_border.h"

#include "cc/paint/paint_flags.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/views/view.h"

namespace ash {

namespace {

// The desk preview border size and padding in dips.
constexpr int kBorderSize = 2;
constexpr int kBorderPadding = 2;

}  // namespace

WmHighlightItemBorder::WmHighlightItemBorder(int corner_radius)
    : views::Border(SK_ColorTRANSPARENT), corner_radius_(corner_radius) {}

void WmHighlightItemBorder::Paint(const views::View& view,
                                  gfx::Canvas* canvas) {
  if (color() == SK_ColorTRANSPARENT)
    return;

  cc::PaintFlags flags;
  flags.setStrokeWidth(kBorderSize);
  flags.setColor(color());
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setAntiAlias(true);

  gfx::RectF bounds(view.GetLocalBounds());
  // The following inset is needed for the rounded corners of the border to
  // look correct. Otherwise, the borders will be painted at the edge of the
  // view, resulting in this border looking chopped. Also, if there is
  // |extra_margin_|, we need to paint it more insetted.
  const int inset = kBorderSize / 2 + extra_margin_;
  bounds.Inset(inset, inset);
  canvas->DrawRoundRect(bounds, corner_radius_, flags);
}

gfx::Insets WmHighlightItemBorder::GetInsets() const {
  return gfx::Insets(kBorderSize + kBorderPadding + extra_margin_);
}

gfx::Size WmHighlightItemBorder::GetMinimumSize() const {
  const int minmum_length = 2 * (kBorderSize + kBorderPadding + extra_margin_);
  return gfx::Size(minmum_length, minmum_length);
}

}  // namespace ash
