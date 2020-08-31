// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/bubble_example.h"

#include <memory>

#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/examples/examples_window.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

using base::ASCIIToUTF16;

namespace views {
namespace examples {

namespace {

SkColor colors[] = {SK_ColorWHITE, SK_ColorGRAY, SK_ColorCYAN, 0xFFC1B1E1};

BubbleBorder::Arrow arrows[] = {
    BubbleBorder::TOP_LEFT,     BubbleBorder::TOP_CENTER,
    BubbleBorder::TOP_RIGHT,    BubbleBorder::RIGHT_TOP,
    BubbleBorder::RIGHT_CENTER, BubbleBorder::RIGHT_BOTTOM,
    BubbleBorder::BOTTOM_RIGHT, BubbleBorder::BOTTOM_CENTER,
    BubbleBorder::BOTTOM_LEFT,  BubbleBorder::LEFT_BOTTOM,
    BubbleBorder::LEFT_CENTER,  BubbleBorder::LEFT_TOP};

base::string16 GetArrowName(BubbleBorder::Arrow arrow) {
  switch (arrow) {
    case BubbleBorder::TOP_LEFT:
      return ASCIIToUTF16("TOP_LEFT");
    case BubbleBorder::TOP_RIGHT:
      return ASCIIToUTF16("TOP_RIGHT");
    case BubbleBorder::BOTTOM_LEFT:
      return ASCIIToUTF16("BOTTOM_LEFT");
    case BubbleBorder::BOTTOM_RIGHT:
      return ASCIIToUTF16("BOTTOM_RIGHT");
    case BubbleBorder::LEFT_TOP:
      return ASCIIToUTF16("LEFT_TOP");
    case BubbleBorder::RIGHT_TOP:
      return ASCIIToUTF16("RIGHT_TOP");
    case BubbleBorder::LEFT_BOTTOM:
      return ASCIIToUTF16("LEFT_BOTTOM");
    case BubbleBorder::RIGHT_BOTTOM:
      return ASCIIToUTF16("RIGHT_BOTTOM");
    case BubbleBorder::TOP_CENTER:
      return ASCIIToUTF16("TOP_CENTER");
    case BubbleBorder::BOTTOM_CENTER:
      return ASCIIToUTF16("BOTTOM_CENTER");
    case BubbleBorder::LEFT_CENTER:
      return ASCIIToUTF16("LEFT_CENTER");
    case BubbleBorder::RIGHT_CENTER:
      return ASCIIToUTF16("RIGHT_CENTER");
    case BubbleBorder::NONE:
      return ASCIIToUTF16("NONE");
    case BubbleBorder::FLOAT:
      return ASCIIToUTF16("FLOAT");
  }
  return ASCIIToUTF16("INVALID");
}

class ExampleBubble : public BubbleDialogDelegateView {
 public:
  ExampleBubble(View* anchor, BubbleBorder::Arrow arrow)
      : BubbleDialogDelegateView(anchor, arrow) {
    DialogDelegate::SetButtons(ui::DIALOG_BUTTON_NONE);
  }

 protected:
  void Init() override {
    SetLayoutManager(std::make_unique<BoxLayout>(
        BoxLayout::Orientation::kVertical, gfx::Insets(50)));
    AddChildView(std::make_unique<Label>(GetArrowName(arrow())));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ExampleBubble);
};

}  // namespace

BubbleExample::BubbleExample() : ExampleBase("Bubble") {}

BubbleExample::~BubbleExample() = default;

void BubbleExample::CreateExampleView(View* container) {
  container->SetLayoutManager(std::make_unique<BoxLayout>(
      BoxLayout::Orientation::kHorizontal, gfx::Insets(), 10));
  no_shadow_ = container->AddChildView(
      std::make_unique<LabelButton>(this, ASCIIToUTF16("No Shadow")));
  no_shadow_opaque_ = container->AddChildView(
      std::make_unique<LabelButton>(this, ASCIIToUTF16("Opaque Border")));
  big_shadow_ = container->AddChildView(
      std::make_unique<LabelButton>(this, ASCIIToUTF16("Big Shadow")));
  small_shadow_ = container->AddChildView(
      std::make_unique<LabelButton>(this, ASCIIToUTF16("Small Shadow")));
  no_assets_ = container->AddChildView(
      std::make_unique<LabelButton>(this, ASCIIToUTF16("No Assets")));
  persistent_ = container->AddChildView(
      std::make_unique<LabelButton>(this, ASCIIToUTF16("Persistent")));
}

void BubbleExample::ButtonPressed(Button* sender, const ui::Event& event) {
  static int arrow_index = 0, color_index = 0;
  static const int count = base::size(arrows);
  arrow_index = (arrow_index + count + (event.IsShiftDown() ? -1 : 1)) % count;
  BubbleBorder::Arrow arrow = arrows[arrow_index];
  if (event.IsControlDown())
    arrow = BubbleBorder::NONE;
  else if (event.IsAltDown())
    arrow = BubbleBorder::FLOAT;

  ExampleBubble* bubble = new ExampleBubble(sender, arrow);
  bubble->set_color(colors[(color_index++) % base::size(colors)]);

  if (sender == no_shadow_)
    bubble->set_shadow(BubbleBorder::NO_SHADOW);
  else if (sender == no_shadow_opaque_)
    bubble->set_shadow(BubbleBorder::NO_SHADOW_OPAQUE_BORDER);
  else if (sender == big_shadow_)
    bubble->set_shadow(BubbleBorder::BIG_SHADOW);
  else if (sender == small_shadow_)
    bubble->set_shadow(BubbleBorder::SMALL_SHADOW);
  else if (sender == no_assets_)
    bubble->set_shadow(BubbleBorder::NO_ASSETS);

  if (sender == persistent_)
    bubble->set_close_on_deactivate(false);

  BubbleDialogDelegateView::CreateBubble(bubble);

  bubble->GetWidget()->Show();
  LogStatus(
      "Click with optional modifiers: [Ctrl] for set_arrow(NONE), "
      "[Alt] for set_arrow(FLOAT), or [Shift] to reverse the arrow iteration.");
}

}  // namespace examples
}  // namespace views
