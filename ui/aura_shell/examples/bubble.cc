// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/utf_string_conversions.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"
#include "views/controls/label.h"

namespace aura_shell {
namespace examples {

struct BubbleConfig {
  string16 label;
  SkColor color;
  views::View* anchor_view;
  views::BubbleBorder::ArrowLocation arrow;
};

class ExampleBubbleDelegateView : public views::BubbleDelegateView {
 public:
  ExampleBubbleDelegateView(const BubbleConfig& config)
      : BubbleDelegateView(config.anchor_view, config.arrow, config.color),
        label_(config.label) {}

  virtual void Init() OVERRIDE {
    SetLayoutManager(new views::FillLayout());
    views::Label* label = new views::Label(label_);
    AddChildView(label);
  }

 private:
  string16 label_;
};

void CreatePointyBubble(views::View* anchor_view) {
  BubbleConfig config;
  config.label = ASCIIToUTF16("PointyBubble");
  config.color = SK_ColorWHITE;
  config.anchor_view = anchor_view;
  config.arrow = views::BubbleBorder::TOP_LEFT;
  ExampleBubbleDelegateView* bubble = new ExampleBubbleDelegateView(config);
  views::BubbleDelegateView::CreateBubble(bubble);
  bubble->Show();
}

}  // namespace examples
}  // namespace aura_shell
