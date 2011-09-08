// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_EXAMPLES_BUBBLE_EXAMPLE_H_
#define VIEWS_EXAMPLES_BUBBLE_EXAMPLE_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/base/animation/animation_delegate.h"
#include "views/controls/button/text_button.h"
#include "views/examples/example_base.h"
#include "views/layout/box_layout.h"
#include "views/layout/grid_layout.h"
#include "views/widget/widget.h"

namespace views {
class BubbleDelegateView;
class Button;
class ColumnSet;
}  // namespace views

namespace examples {
struct BubbleConfig;

// A Bubble example.
class BubbleExample : public ExampleBase,
                      public ui::AnimationDelegate,
                      public views::ButtonListener {
 public:
  explicit BubbleExample(ExamplesMain* main);
  virtual ~BubbleExample();

  // Overridden from ExampleBase.
  virtual std::wstring GetExampleTitle() OVERRIDE;
  virtual void CreateExampleView(views::View* container) OVERRIDE;

 private:
  virtual void AnimationEnded(const ui::Animation* animation) OVERRIDE;

  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event) OVERRIDE;

  // Add a bubble view in the example
  views::Widget* AddBubbleButton(const BubbleConfig& config);

  views::Button* round_;
  views::Button* pointy_;
  views::Button* fade_;
  views::BoxLayout* layout_;

  views::Widget* round_bubble_;
  views::Widget* pointy_bubble_;
  views::Widget* fade_bubble_;

  DISALLOW_COPY_AND_ASSIGN(BubbleExample);
};

}  // namespace examples

#endif  // VIEWS_EXAMPLES_BUBBLE_EXAMPLE_H_
