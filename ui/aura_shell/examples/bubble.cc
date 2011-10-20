// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/utf_string_conversions.h"
#include "views/bubble/bubble_border.h"
#include "views/bubble/bubble_delegate.h"
#include "views/controls/label.h"
#include "views/layout/fill_layout.h"
#include "views/widget/widget.h"

namespace aura_shell {
namespace examples {

struct BubbleConfig {
  string16 label;
  SkColor color;
  gfx::Point anchor_point;
  views::BubbleBorder::ArrowLocation arrow;
};

class ExampleBubbleDelegateView : public views::BubbleDelegateView {
 public:
  ExampleBubbleDelegateView(const BubbleConfig& config)
      : BubbleDelegateView(config.anchor_point, config.arrow, config.color),
        label_(config.label) {}

  virtual void Init() OVERRIDE {
    SetLayoutManager(new views::FillLayout());
    views::Label* label = new views::Label(label_);
    label->set_border(views::Border::CreateSolidBorder(10, GetColor()));
    AddChildView(label);
  }

 private:
  string16 label_;
};

void CreatePointyBubble(views::Widget* parent, const gfx::Point& origin) {
  BubbleConfig config;
  config.label = ASCIIToUTF16("PointyBubble");
  config.color = SK_ColorWHITE;
  config.anchor_point = origin;
  config.arrow = views::BubbleBorder::TOP_LEFT;
  views::Widget* bubble = views::BubbleDelegateView::CreateBubble(
      new ExampleBubbleDelegateView(config), parent);
  bubble->Show();
}

}  // namespace examples
}  // namespace aura_shell
