// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_RADIO_BUTTON_EXAMPLE_H_
#define UI_VIEWS_EXAMPLES_RADIO_BUTTON_EXAMPLE_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/views/controls/button/radio_button.h"
#include "ui/views/examples/example_base.h"

namespace events {
class TextButton;
}

namespace examples {

class RadioButtonExample : public ExampleBase,
                           public views::ButtonListener {
 public:
  explicit RadioButtonExample(ExamplesMain* main);
  virtual ~RadioButtonExample();

  // Overridden from ExampleBase:
  virtual void CreateExampleView(views::View* container) OVERRIDE;

 private:
  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event) OVERRIDE;

  // Group of 3 radio buttons.
  views::RadioButton* radio_buttons_[3];

  // Control button to select radio buttons, and show the status of buttons.
  views::TextButton* select_;
  views::TextButton* status_;

  // The number of times the button is pressed.
  int count_;

  DISALLOW_COPY_AND_ASSIGN(RadioButtonExample);
};

}  // namespace examples

#endif  // UI_VIEWS_EXAMPLES_RADIO_BUTTON_EXAMPLE_H_
