// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/bubble_example.h"

#include "base/utf_string_conversions.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/controls/button/text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

namespace views {
namespace examples {

struct BubbleConfig {
  string16 label;
  SkColor color;
  View* anchor_view;
  BubbleBorder::ArrowLocation arrow;
  bool fade_in;
  bool fade_out;
};

// Create four types of bubbles, one without arrow, one with an arrow, one
// that fades in, and another that fades out and won't close on the escape key.
BubbleConfig kRoundConfig = { ASCIIToUTF16("Round"), 0xFFC1B1E1, NULL,
                              BubbleBorder::NONE, false, false };
BubbleConfig kArrowConfig = { ASCIIToUTF16("Arrow"), SK_ColorGRAY, NULL,
                              BubbleBorder::TOP_LEFT, false, false };
BubbleConfig kFadeInConfig = { ASCIIToUTF16("FadeIn"), SK_ColorYELLOW, NULL,
                               BubbleBorder::BOTTOM_RIGHT, true, false };
BubbleConfig kFadeOutConfig = { ASCIIToUTF16("FadeOut"), SK_ColorWHITE, NULL,
                                BubbleBorder::LEFT_TOP, false, true };

class ExampleBubbleDelegateView : public BubbleDelegateView {
 public:
  explicit ExampleBubbleDelegateView(const BubbleConfig& config)
      : BubbleDelegateView(config.anchor_view, config.arrow),
        label_(config.label) {
    set_color(config.color);
  }

 protected:
  virtual void Init() OVERRIDE {
    SetLayoutManager(new FillLayout());
    Label* label = new Label(label_);
    AddChildView(label);
  }

 private:
  string16 label_;
};

BubbleExample::BubbleExample() : ExampleBase("Bubble") {}

BubbleExample::~BubbleExample() {}

void BubbleExample::CreateExampleView(View* container) {
  container->SetLayoutManager(
      new BoxLayout(BoxLayout::kHorizontal, 0, 0, 1));
  round_ = new TextButton(this, kRoundConfig.label);
  arrow_ = new TextButton(this, kArrowConfig.label);
  fade_in_ = new TextButton(this, kFadeInConfig.label);
  fade_out_ = new TextButton(this, kFadeOutConfig.label);
  container->AddChildView(round_);
  container->AddChildView(arrow_);
  container->AddChildView(fade_in_);
  container->AddChildView(fade_out_);
}

void BubbleExample::ButtonPressed(Button* sender, const Event& event) {
  BubbleConfig config;
  if (sender == round_)
    config = kRoundConfig;
  else if (sender == arrow_)
    config = kArrowConfig;
  else if (sender == fade_in_)
    config = kFadeInConfig;
  else if (sender == fade_out_)
    config = kFadeOutConfig;

  config.anchor_view = sender;
  ExampleBubbleDelegateView* bubble_delegate =
      new ExampleBubbleDelegateView(config);
  BubbleDelegateView::CreateBubble(bubble_delegate);

  if (config.fade_in)
    bubble_delegate->StartFade(true);
  else
    bubble_delegate->Show();

  if (config.fade_out) {
    bubble_delegate->set_close_on_esc(false);
    bubble_delegate->StartFade(false);
  }
}

}  // namespace examples
}  // namespace views
