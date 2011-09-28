// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/examples/bubble_example.h"

#include "third_party/skia/include/core/SkColor.h"
#include "views/bubble/bubble_border.h"
#include "views/bubble/bubble_delegate.h"
#include "views/bubble/bubble_view.h"
#include "views/controls/button/checkbox.h"
#include "views/controls/button/text_button.h"
#include "views/controls/label.h"
#include "views/layout/box_layout.h"
#include "views/view.h"
#include "views/widget/widget.h"

namespace examples {

struct BubbleConfig {
  std::wstring label;
  SkColor color;
  gfx::Rect bound;
  views::BubbleBorder::ArrowLocation arrow;
  bool fade_out;
};

// Create three types of bubbles, one without arrow, one with
// arrow, and the third has a fade animation.
BubbleConfig kRoundBubbleConfig = {
  L"RoundBubble", 0xFFC1B1E1, gfx::Rect(50, 350, 100, 50),
    views::BubbleBorder::NONE, false };
BubbleConfig kPointyBubbleConfig = {
  L"PointyBubble", SK_ColorLTGRAY, gfx::Rect(250, 350, 180, 180),
    views::BubbleBorder::TOP_LEFT, false };
BubbleConfig kFadeBubbleConfig = {
  L"FadeBubble", SK_ColorYELLOW, gfx::Rect(500, 350, 200, 100),
    views::BubbleBorder::BOTTOM_RIGHT, true };

class ExampleBubbleDelegateView : public views::BubbleDelegateView {
 public:
  ExampleBubbleDelegateView(const BubbleConfig& config, views::Widget* widget)
      : BubbleDelegateView(widget),config_(config) {}

  virtual views::View* GetContentsView() { return this; }

  SkColor GetFrameBackgroundColor() {
    return config_.color;
  }
  gfx::Rect GetBounds() {
    return config_.bound;
  }

  views::BubbleBorder::ArrowLocation GetFrameArrowLocation() {
    return config_.arrow;
 }

 private:
  const BubbleConfig& config_;
};

BubbleExample::BubbleExample(ExamplesMain* main)
    : ExampleBase(main, "Bubble") {
}

BubbleExample::~BubbleExample() {
}

void BubbleExample::CreateExampleView(views::View* container) {
  layout_ = new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0, 1);
  container->SetLayoutManager(layout_);
  round_ = new views::TextButton(this, kRoundBubbleConfig.label);
  pointy_ = new views::TextButton(this, kPointyBubbleConfig.label);
  fade_ = new views::TextButton(this, kFadeBubbleConfig.label);
  container->AddChildView(round_);
  container->AddChildView(pointy_);
  container->AddChildView(fade_);
}

void BubbleExample::ButtonPressed(views::Button* sender,
                                  const views::Event& event) {
  if (sender == round_) {
    round_bubble_ = AddBubbleButton(kRoundBubbleConfig);
    round_bubble_->Show();
  } else if (sender == pointy_) {
    pointy_bubble_ = AddBubbleButton(kPointyBubbleConfig);
    pointy_bubble_->Show();
  } else if (sender == fade_) {
    fade_bubble_ = AddBubbleButton(kFadeBubbleConfig);
    fade_bubble_->Show();
    // Start fade animation.
    fade_bubble_->client_view()->AsBubbleView()->set_animation_delegate(this);
    fade_bubble_->client_view()->AsBubbleView()->StartFade();
  }
  GetExampleView()->SchedulePaint();
}

views::Widget* BubbleExample::AddBubbleButton(const BubbleConfig& config) {
  // Create a Label.
  views::Label* bubble_message = new views::Label(L"I am a " + config.label);
  bubble_message->SetMultiLine(true);
  bubble_message->SetAllowCharacterBreak(true);
  bubble_message->SetHorizontalAlignment(views::Label::ALIGN_LEFT);

  views::Widget* bubble_widget = new views::Widget();
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_BUBBLE);
  params.delegate = new ExampleBubbleDelegateView(config, bubble_widget);
  params.transparent = true;
  params.bounds = config.bound;
  params.parent = GetExampleView()->GetWidget()->GetNativeView();
  bubble_widget->Init(params);
  bubble_widget->client_view()->AsBubbleView()->AddChildView(bubble_message);
  return bubble_widget;
}

void BubbleExample::AnimationEnded(const ui::Animation* animation) {
  PrintStatus("Done Bubble Animation");
}

}  // namespace examples
