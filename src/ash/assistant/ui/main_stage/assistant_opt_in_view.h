// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_OPT_IN_VIEW_H_
#define ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_OPT_IN_VIEW_H_

#include "base/macros.h"
#include "ui/views/controls/button/button.h"

namespace views {
class StyledLabel;
}  // namespace views

namespace ash {

// AssistantOptInDelegate ------------------------------------------------------

class AssistantOptInDelegate {
 public:
  // Invoked when the Assistant opt in button is pressed.
  virtual void OnOptInButtonPressed() = 0;

 protected:
  virtual ~AssistantOptInDelegate() = default;
};

// AssistantOptInView ----------------------------------------------------------

class AssistantOptInView : public views::View, public views::ButtonListener {
 public:
  AssistantOptInView();
  ~AssistantOptInView() override;

  // views::View:
  const char* GetClassName() const override;
  void ChildPreferredSizeChanged(views::View* child) override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  void set_delegate(AssistantOptInDelegate* delegate) { delegate_ = delegate; }

 private:
  void InitLayout();

  views::StyledLabel* label_;  // Owned by view hierarchy.

  AssistantOptInDelegate* delegate_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(AssistantOptInView);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_OPT_IN_VIEW_H_
