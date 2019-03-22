// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_CONTROL_BUTTON_H_
#define ASH_SHELF_SHELF_CONTROL_BUTTON_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/macros.h"
#include "ui/views/controls/button/image_button.h"

namespace ash {

// Base class for controls shown on the shelf that are not app shortcuts, such
// as the app list, back, and overflow buttons.
class ASH_EXPORT ShelfControlButton : public views::ImageButton {
 public:
  ShelfControlButton();
  ~ShelfControlButton() override;

  // Get the center point of the button used to draw its background and ink
  // drops.
  gfx::Point GetCenterPoint() const;

 protected:
  // views::ImageButton:
  std::unique_ptr<views::InkDropRipple> CreateInkDropRipple() const override;
  std::unique_ptr<views::InkDrop> CreateInkDrop() override;
  std::unique_ptr<views::InkDropMask> CreateInkDropMask() const override;

  void PaintBackground(gfx::Canvas* canvas, const gfx::Rect& bounds);

  DISALLOW_COPY_AND_ASSIGN(ShelfControlButton);
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_CONTROL_BUTTON_H_
