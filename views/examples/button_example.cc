// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/examples/button_example.h"

#include "grit/ui_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "views/controls/button/checkbox.h"
#include "views/layout/fill_layout.h"
#include "views/view.h"

namespace examples {

ButtonExample::ButtonExample(ExamplesMain* main)
    : ExampleBase(main, "Text Button"),
      alignment_(views::TextButton::ALIGN_LEFT),
      use_native_theme_border_(false),
      icon_(NULL),
      count_(0) {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  icon_ = rb.GetBitmapNamed(IDR_CLOSE_SA_H);
}

ButtonExample::~ButtonExample() {
}

void ButtonExample::CreateExampleView(views::View* container) {
  views::TextButton* tb = new views::TextButton(this, L"Button");
  button_ = tb;
  container->SetLayoutManager(new views::FillLayout);
  container->AddChildView(button_);
}

void ButtonExample::ButtonPressed(views::Button* sender,
                                  const views::Event& event) {
  PrintStatus("Pressed! count:%d", ++count_);

  if (event.IsControlDown()) {
    if (event.IsShiftDown()) {
      if (event.IsAltDown()) {
        button_->SetMultiLine(!button_->multi_line());
        if (button_->multi_line()) {
          button_->SetText(L"Multi-line text\n"
                           L"is here to stay all the way!\n"
                           L"123");
        } else {
          button_->SetText(L"Button");
        }
      } else {
        switch(button_->icon_placement()) {
          case views::TextButton::ICON_ON_LEFT:
            button_->set_icon_placement(views::TextButton::ICON_ON_RIGHT);
            break;
          case views::TextButton::ICON_ON_RIGHT:
            button_->set_icon_placement(views::TextButton::ICON_ON_LEFT);
            break;
        }
      }
    } else if (event.IsAltDown()) {
      if (button_->HasIcon())
        button_->SetIcon(SkBitmap());
      else
        button_->SetIcon(*icon_);
    } else {
      switch(alignment_) {
        case views::TextButton::ALIGN_LEFT:
          alignment_ = views::TextButton::ALIGN_CENTER;
          break;
        case views::TextButton::ALIGN_CENTER:
          alignment_ = views::TextButton::ALIGN_RIGHT;
          break;
        case views::TextButton::ALIGN_RIGHT:
          alignment_ = views::TextButton::ALIGN_LEFT;
          break;
      }
      button_->set_alignment(alignment_);
    }
  } else if (event.IsShiftDown()) {
    if (event.IsAltDown()) {
      if (button_->text().length() < 10) {
        button_->SetText(L"Startof"
                         L"ReallyReallyReallyReallyReallyReallyReally"
                         L"ReallyReallyReallyReallyReallyReallyReally"
                         L"ReallyReallyReallyReallyReallyReallyReally"
                         L"LongButtonText");
      } else {
        button_->SetText(L"Button");
      }
    } else {
      use_native_theme_border_ = !use_native_theme_border_;
      if (use_native_theme_border_)
        button_->set_border(new views::TextButtonNativeThemeBorder(button_));
      else
        button_->set_border(new views::TextButtonBorder());
    }
  } else if (event.IsAltDown()) {
    button_->SetIsDefault(!button_->is_default());
  }
}

}  // namespace examples
