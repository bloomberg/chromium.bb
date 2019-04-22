// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_CLOSE_DESK_BUTTON_H_
#define ASH_WM_DESKS_CLOSE_DESK_BUTTON_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/macros.h"
#include "ui/views/controls/button/image_button.h"

namespace ash {

// A button view that shows up on hovering over the associated desk mini_view,
// which let users remove the mini_view and its corresponding desk.
class ASH_EXPORT CloseDeskButton : public views::ImageButton {
 public:
  explicit CloseDeskButton(views::ButtonListener* listener);
  ~CloseDeskButton() override;

  // views::ImageButton:
  const char* GetClassName() const override;
  std::unique_ptr<views::InkDrop> CreateInkDrop() override;
  std::unique_ptr<views::InkDropRipple> CreateInkDropRipple() const override;
  std::unique_ptr<views::InkDropHighlight> CreateInkDropHighlight()
      const override;
  SkColor GetInkDropBaseColor() const override;
  std::unique_ptr<views::InkDropMask> CreateInkDropMask() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(CloseDeskButton);
};

}  // namespace ash

#endif  // ASH_WM_DESKS_CLOSE_DESK_BUTTON_H_
