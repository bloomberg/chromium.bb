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

namespace views {
namespace examples {

ButtonExample::ButtonExample()
    : ExampleBase("Button"),
      text_button_(NULL),
      label_button_(NULL),
      image_button_(NULL),
      alignment_(TextButton::ALIGN_LEFT),
      use_native_theme_border_(false),
      icon_(NULL),
      count_(0) {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  icon_ = rb.GetImageNamed(IDR_CLOSE_SA_H).ToImageSkia();
}

ButtonExample::~ButtonExample() {
}

void ButtonExample::CreateExampleView(View* container) {
  container->SetLayoutManager(new BoxLayout(BoxLayout::kVertical, 0, 0, 10));

  text_button_ = new TextButton(this, ASCIIToUTF16("Text Button"));
  text_button_->set_focusable(true);
  text_button_->SetHoverColor(SK_ColorRED);
  container->AddChildView(text_button_);

  label_button_ = new LabelButton(this, ASCIIToUTF16("Label Button"));
  label_button_->set_focusable(true);
  label_button_->SetTextColor(CustomButton::STATE_HOVERED, SK_ColorRED);
  container->AddChildView(label_button_);

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  image_button_ = new ImageButton(this);
  image_button_->set_focusable(true);
  image_button_->SetImage(ImageButton::STATE_NORMAL,
                          rb.GetImageNamed(IDR_CLOSE).ToImageSkia());
  image_button_->SetImage(ImageButton::STATE_HOVERED,
                          rb.GetImageNamed(IDR_CLOSE_H).ToImageSkia());
  image_button_->SetImage(ImageButton::STATE_PRESSED,
                          rb.GetImageNamed(IDR_CLOSE_P).ToImageSkia());
  image_button_->SetOverlayImage(rb.GetImageNamed(
      IDR_MENU_CHECK).ToImageSkia());
  container->AddChildView(image_button_);
}

void ButtonExample::TextButtonPressed(const ui::Event& event) {
  PrintStatus("Text Button Pressed! count: %d", ++count_);
  if (event.IsControlDown()) {
    if (event.IsShiftDown()) {
      if (event.IsAltDown()) {
        text_button_->SetMultiLine(!text_button_->multi_line());
        if (text_button_->multi_line()) {
          text_button_->SetText(ASCIIToUTF16("Multi-line text\n") +
                                ASCIIToUTF16("is here to stay all the way!\n") +
                                ASCIIToUTF16("123"));
        } else {
          text_button_->SetText(ASCIIToUTF16("Text Button"));
        }
      } else {
        switch (text_button_->icon_placement()) {
          case TextButton::ICON_ON_LEFT:
            text_button_->set_icon_placement(TextButton::ICON_ON_RIGHT);
            break;
          case TextButton::ICON_ON_RIGHT:
            text_button_->set_icon_placement(TextButton::ICON_ON_LEFT);
            break;
          case TextButton::ICON_CENTERED:
            // Do nothing.
            break;
        }
      }
    } else if (event.IsAltDown()) {
      if (text_button_->HasIcon())
        text_button_->SetIcon(gfx::ImageSkia());
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
      if (text_button_->text().length() < 20) {
        text_button_->SetText(
            ASCIIToUTF16("Startof") +
            ASCIIToUTF16("ReallyReallyReallyReallyReallyReallyReally") +
            ASCIIToUTF16("ReallyReallyReallyReallyReallyReallyReally") +
            ASCIIToUTF16("ReallyReallyReallyReallyReallyReallyReally") +
            ASCIIToUTF16("LongButtonText"));
      } else {
        text_button_->SetText(ASCIIToUTF16("Text Button"));
      }
    } else {
      use_native_theme_border_ = !use_native_theme_border_;
      if (use_native_theme_border_)
        text_button_->set_border(new TextButtonNativeThemeBorder(text_button_));
      else
        text_button_->set_border(new TextButtonDefaultBorder());
    }
  } else if (event.IsAltDown()) {
    text_button_->SetIsDefault(!text_button_->is_default());
  } else {
    text_button_->ClearMaxTextSize();
  }
  example_view()->GetLayoutManager()->Layout(example_view());
}

void ButtonExample::LabelButtonPressed(const ui::Event& event) {
  PrintStatus("Label Button Pressed! count: %d", ++count_);
  if (event.IsControlDown()) {
    if (event.IsShiftDown()) {
      if (event.IsAltDown()) {
        label_button_->SetTextMultiLine(!label_button_->GetTextMultiLine());
        label_button_->SetText(ASCIIToUTF16(label_button_->GetTextMultiLine() ?
            "Multi-line text\nis here to stay all the way!\n123" :
            "Label Button"));
      } else {
        label_button_->SetText(!label_button_->GetText().empty() ?
            string16() : ASCIIToUTF16("Label Button"));
      }
    } else if (event.IsAltDown()) {
      label_button_->SetImage(CustomButton::STATE_NORMAL,
          label_button_->GetImage(CustomButton::STATE_NORMAL).isNull() ?
          *icon_ : gfx::ImageSkia());
    } else {
      label_button_->SetHorizontalAlignment(
          static_cast<gfx::HorizontalAlignment>(
              (label_button_->GetHorizontalAlignment() + 1) % 3));
    }
  } else if (event.IsShiftDown()) {
    if (event.IsAltDown()) {
      label_button_->SetText(ASCIIToUTF16(
          label_button_->GetText().length() > 20 ? "Label Button" :
          "StartofReallyReallyReallyReallyReallyReallyReally"
          "ReallyReallyReallyReallyReallyReallyReally"
          "ReallyReallyReallyReallyReallyReallyReallyLongButtonText"));
    } else {
      label_button_->SetNativeTheme(!label_button_->native_theme());
    }
  } else if (event.IsAltDown()) {
    label_button_->SetIsDefault(!label_button_->is_default());
  } else {
    label_button_->set_min_size(gfx::Size());
  }
  example_view()->GetLayoutManager()->Layout(example_view());
}

void ButtonExample::ButtonPressed(Button* sender, const ui::Event& event) {
  if (sender == text_button_)
    TextButtonPressed(event);
  else if (sender == label_button_)
    LabelButtonPressed(event);
  else
    PrintStatus("Image Button Pressed! count: %d", ++count_);
}

}  // namespace examples
}  // namespace views
