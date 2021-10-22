// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_LOGIN_TOOLTIP_VIEW_H_
#define ASH_LOGIN_UI_LOGIN_TOOLTIP_VIEW_H_

#include "ash/login/ui/login_base_bubble_view.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/views/view.h"

namespace views {
class Label;
}  // namespace views

namespace ash {

class LoginTooltipView : public LoginBaseBubbleView {
 public:
  LoginTooltipView(const std::u16string& message, views::View* anchor_view);

  LoginTooltipView(const LoginTooltipView&) = delete;
  LoginTooltipView& operator=(const LoginTooltipView&) = delete;

  ~LoginTooltipView() override;

  void set_text(const std::u16string& message) { label_->SetText(message); }

  void UpdateIcon();

  // LoginBaseBubbleView:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void OnThemeChanged() override;

 protected:
  views::Label* label() { return label_; }

 private:
  views::Label* label_ = nullptr;
  views::ImageView* info_icon_ = nullptr;
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_LOGIN_TOOLTIP_VIEW_H_
