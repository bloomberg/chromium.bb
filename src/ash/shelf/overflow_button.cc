// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/overflow_button.h"

#include <memory>

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_constants.h"
#include "ash/shelf/shelf_view.h"
#include "ash/strings/grit/ash_strings.h"
#include "chromeos/chromeos_switches.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/skbitmap_operations.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

OverflowButton::OverflowButton(ShelfView* shelf_view, Shelf* shelf)
    : ShelfControlButton(), shelf_view_(shelf_view), shelf_(shelf) {
  DCHECK(shelf_view_);

  set_hide_ink_drop_when_showing_context_menu(false);

  SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
  SetAccessibleName(l10n_util::GetStringUTF16(IDS_ASH_SHELF_OVERFLOW_NAME));

  horizontal_dots_image_view_ = new views::ImageView();
  horizontal_dots_image_view_->SetImage(
      gfx::CreateVectorIcon(kShelfOverflowHorizontalDotsIcon, kShelfIconColor));
  SetLayoutManager(std::make_unique<views::FillLayout>());
  AddChildView(horizontal_dots_image_view_);
}

OverflowButton::~OverflowButton() = default;

bool OverflowButton::ShouldEnterPushedState(const ui::Event& event) {
  if (shelf_view_->IsShowingOverflowBubble())
    return false;

  return Button::ShouldEnterPushedState(event);
}

void OverflowButton::NotifyClick(const ui::Event& event) {
  shelf_view_->ButtonPressed(this, event, nullptr);
}

void OverflowButton::PaintButtonContents(gfx::Canvas* canvas) {
  gfx::Rect bounds = CalculateButtonBounds();
  PaintBackground(canvas, bounds);
}

gfx::Rect OverflowButton::CalculateButtonBounds() const {
  ShelfAlignment alignment = shelf_->alignment();
  gfx::Rect content_bounds = GetContentsBounds();
  // Align the button to the top of a bottom-aligned shelf, to the right edge
  // a left-aligned shelf, and to the left edge of a right-aligned shelf.
  const int inset = (ShelfConstants::shelf_size() - kShelfControlSize) / 2;
  const int x = alignment == SHELF_ALIGNMENT_LEFT
                    ? content_bounds.right() - inset - kShelfControlSize
                    : content_bounds.x() + inset;
  return gfx::Rect(x, content_bounds.y() + inset, kShelfControlSize,
                   kShelfControlSize);
}

}  // namespace ash
