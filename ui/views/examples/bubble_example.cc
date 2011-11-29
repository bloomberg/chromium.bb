// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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

namespace examples {

struct BubbleConfig {
  string16 label;
  SkColor color;
  views::View* anchor_view;
  views::BubbleBorder::ArrowLocation arrow;
  bool fade_in;
  bool fade_out;
};

// Create four types of bubbles, one without arrow, one with an arrow, one
// that fades in, and another that fades out and won't close on the escape key.
BubbleConfig kRoundConfig = { ASCIIToUTF16("Round"), 0xFFC1B1E1, NULL,
                              views::BubbleBorder::NONE, false, false };
BubbleConfig kArrowConfig = { ASCIIToUTF16("Arrow"), SK_ColorGRAY, NULL,
                              views::BubbleBorder::TOP_LEFT, false, false };
BubbleConfig kFadeInConfig = { ASCIIToUTF16("FadeIn"), SK_ColorYELLOW, NULL,
                               views::BubbleBorder::BOTTOM_RIGHT, true, false };
BubbleConfig kFadeOutConfig = { ASCIIToUTF16("FadeOut"), SK_ColorWHITE, NULL,
                                views::BubbleBorder::LEFT_TOP, false, true };

class ExampleBubbleDelegateView : public views::BubbleDelegateView {
 public:
  ExampleBubbleDelegateView(const BubbleConfig& config)
      : BubbleDelegateView(config.anchor_view, config.arrow, config.color),
        label_(config.label) {}

 protected:
  virtual void Init() OVERRIDE {
    SetLayoutManager(new views::FillLayout());
    views::Label* label = new views::Label(label_);
    AddChildView(label);
  }

 private:
  string16 label_;
};

BubbleExample::BubbleExample(ExamplesMain* main)
    : ExampleBase(main, "Bubble") {}

BubbleExample::~BubbleExample() {}

void BubbleExample::CreateExampleView(views::View* container) {
  container->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0, 1));
  round_ = new views::TextButton(this, kRoundConfig.label);
  arrow_ = new views::TextButton(this, kArrowConfig.label);
  fade_in_ = new views::TextButton(this, kFadeInConfig.label);
  fade_out_ = new views::TextButton(this, kFadeOutConfig.label);
  container->AddChildView(round_);
  container->AddChildView(arrow_);
  container->AddChildView(fade_in_);
  container->AddChildView(fade_out_);
}

void BubbleExample::ButtonPressed(views::Button* sender,
                                  const views::Event& event) {
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
  views::BubbleDelegateView::CreateBubble(bubble_delegate);

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
