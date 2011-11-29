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

namespace views {
namespace examples {

class RadioButtonExample : public ExampleBase, public ButtonListener {
 public:
  RadioButtonExample();
  virtual ~RadioButtonExample();

  // Overridden from ExampleBase:
  virtual void CreateExampleView(View* container) OVERRIDE;

 private:
  // Overridden from ButtonListener:
  virtual void ButtonPressed(Button* sender, const Event& event) OVERRIDE;

  // Group of 3 radio buttons.
  RadioButton* radio_buttons_[3];

  // Control button to select radio buttons, and show the status of buttons.
  TextButton* select_;
  TextButton* status_;

  // The number of times the button is pressed.
  int count_;

  DISALLOW_COPY_AND_ASSIGN(RadioButtonExample);
};

}  // namespace examples
}  // namespace views

#endif  // UI_VIEWS_EXAMPLES_RADIO_BUTTON_EXAMPLE_H_
