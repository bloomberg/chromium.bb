// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/button_example.h"

#include "base/utf_string_conversions.h"
#include "grit/ui_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/text_button.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"

namespace {
const int kLayoutSpacing = 10;  // pixels
}  // namespace

namespace views {
namespace examples {

ButtonExample::ButtonExample()
    : ExampleBase("Button"),
      text_button_(NULL),
      image_button_(NULL),
      alignment_(TextButton::ALIGN_LEFT),
      use_native_theme_border_(false),
      icon_(NULL),
      count_(0) {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  icon_ = rb.GetImageNamed(IDR_CLOSE_SA_H).ToSkBitmap();
}

ButtonExample::~ButtonExample() {
}

void ButtonExample::CreateExampleView(View* container) {
  container->SetLayoutManager(
      new BoxLayout(BoxLayout::kVertical, 0, 0, kLayoutSpacing));

  text_button_ = new TextButton(this, ASCIIToUTF16("Text Button"));
  container->AddChildView(text_button_);

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  image_button_ = new ImageButton(this);
  image_button_->SetImage(ImageButton::BS_NORMAL,
                          rb.GetImageNamed(IDR_CLOSE).ToImageSkia());
  image_button_->SetImage(ImageButton::BS_HOT,
                          rb.GetImageNamed(IDR_CLOSE_H).ToImageSkia());
  image_button_->SetImage(ImageButton::BS_PUSHED,
                          rb.GetImageNamed(IDR_CLOSE_P).ToImageSkia());
  image_button_->SetOverlayImage(rb.GetImageNamed(IDR_MENU_CHECK).ToSkBitmap());
  container->AddChildView(image_button_);
}

void ButtonExample::ButtonPressed(Button* sender, const Event& event) {
  PrintStatus("Pressed! count: %d", ++count_);

  if (event.IsControlDown()) {
    if (event.IsShiftDown()) {
      if (event.IsAltDown()) {
        text_button_->SetMultiLine(!text_button_->multi_line());
        if (text_button_->multi_line()) {
          text_button_->SetText(ASCIIToUTF16("Multi-line text\n") +
                           ASCIIToUTF16("is here to stay all the way!\n") +
                           ASCIIToUTF16("123"));
        } else {
          text_button_->SetText(ASCIIToUTF16("Button"));
        }
      } else {
        switch (text_button_->icon_placement()) {
          case TextButton::ICON_ON_LEFT:
            text_button_->set_icon_placement(TextButton::ICON_ON_RIGHT);
            break;
          case TextButton::ICON_ON_RIGHT:
            text_button_->set_icon_placement(TextButton::ICON_ON_LEFT);
            break;
        }
      }
    } else if (event.IsAltDown()) {
      if (text_button_->HasIcon())
        text_button_->SetIcon(SkBitmap());
      else
        text_button_->SetIcon(*icon_);
    } else {
      switch (alignment_) {
        case TextButton::ALIGN_LEFT:
          alignment_ = TextButton::ALIGN_CENTER;
          break;
        case TextButton::ALIGN_CENTER:
          alignment_ = TextButton::ALIGN_RIGHT;
          break;
        case TextButton::ALIGN_RIGHT:
          alignment_ = TextButton::ALIGN_LEFT;
          break;
      }
      text_button_->set_alignment(alignment_);
    }
  } else if (event.IsShiftDown()) {
    if (event.IsAltDown()) {
      if (text_button_->text().length() < 10) {
        text_button_->SetText(
            ASCIIToUTF16("Startof") +
            ASCIIToUTF16("ReallyReallyReallyReallyReallyReallyReally") +
            ASCIIToUTF16("ReallyReallyReallyReallyReallyReallyReally") +
            ASCIIToUTF16("ReallyReallyReallyReallyReallyReallyReally") +
            ASCIIToUTF16("LongButtonText"));
      } else {
        text_button_->SetText(ASCIIToUTF16("Button"));
      }
    } else {
      use_native_theme_border_ = !use_native_theme_border_;
      if (use_native_theme_border_)
        text_button_->set_border(new TextButtonNativeThemeBorder(text_button_));
      else
        text_button_->set_border(new TextButtonBorder());
    }
  } else if (event.IsAltDown()) {
    text_button_->SetIsDefault(!text_button_->is_default());
  }
  example_view()->GetLayoutManager()->Layout(example_view());
}

}  // namespace examples
}  // namespace views
