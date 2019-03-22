// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_BASE_ASSISTANT_BUTTON_H_
#define ASH_ASSISTANT_UI_BASE_ASSISTANT_BUTTON_H_

#include <memory>

#include "base/macros.h"
#include "ui/views/controls/button/image_button.h"

namespace ash {

class AssistantButton : public views::ImageButton {
 public:
  explicit AssistantButton(views::ButtonListener* listener);
  ~AssistantButton() override;

  // views::Button:
  const char* GetClassName() const override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  std::unique_ptr<views::InkDrop> CreateInkDrop() override;
  std::unique_ptr<views::InkDropHighlight> CreateInkDropHighlight()
      const override;
  std::unique_ptr<views::InkDropMask> CreateInkDropMask() const override;
  std::unique_ptr<views::InkDropRipple> CreateInkDropRipple() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(AssistantButton);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_BASE_ASSISTANT_BUTTON_H_
