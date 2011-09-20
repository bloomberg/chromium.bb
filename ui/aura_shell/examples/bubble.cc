// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/utf_string_conversions.h"
#include "views/bubble/bubble_border.h"
#include "views/bubble/bubble_delegate.h"
#include "views/bubble/bubble_view.h"
#include "views/controls/label.h"
#include "views/widget/widget.h"

namespace aura_shell {
namespace examples {

struct BubbleConfig {
  string16 label;
  SkColor color;
  gfx::Rect bound;
  views::BubbleBorder::ArrowLocation arrow;
  bool fade_out;
};

class ExampleBubbleDelegateView : public views::BubbleDelegateView {
 public:
  ExampleBubbleDelegateView(const BubbleConfig& config, views::Widget* widget)
      : BubbleDelegateView(widget),
        config_(config) {}
  virtual ~ExampleBubbleDelegateView() {}

  // Overridden from views::BubbleDelegateView
  virtual views::View* GetContentsView() OVERRIDE { return this; }
  virtual SkColor GetFrameBackgroundColor() OVERRIDE { return config_.color; }
  virtual gfx::Rect GetBounds() OVERRIDE { return config_.bound; }
  virtual views::BubbleBorder::ArrowLocation GetFrameArrowLocation() OVERRIDE {
    return config_.arrow;
 }

 private:
  const BubbleConfig config_;

  DISALLOW_COPY_AND_ASSIGN(ExampleBubbleDelegateView);
};

void CreateBubble(const BubbleConfig& config,
                  gfx::NativeWindow parent) {
  views::Widget* bubble_widget = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_BUBBLE);
  params.delegate = new ExampleBubbleDelegateView(config, bubble_widget);
  params.transparent = true;
  params.bounds = config.bound;
  params.parent = parent;
  bubble_widget->Init(params);
  bubble_widget->client_view()->AsBubbleView()->AddChildView(
      new views::Label(L"I am a " + config.label));
}

void CreatePointyBubble(gfx::NativeWindow parent, const gfx::Point& origin) {
  BubbleConfig config;
  config.label = ASCIIToUTF16("PointyBubble");
  config.color = SK_ColorWHITE;
  config.bound = gfx::Rect(origin.x(), origin.y(), 180, 180);
  config.arrow = views::BubbleBorder::TOP_LEFT;
  config.fade_out = false;
  CreateBubble(config, parent);
}

}  // namespace examples
}  // namespace aura_shell
