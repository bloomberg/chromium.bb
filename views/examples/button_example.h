// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_EXAMPLES_BUTTON_EXAMPLE_H_
#define VIEWS_EXAMPLES_BUTTON_EXAMPLE_H_

#include "base/string_util.h"
#include "views/controls/button/text_button.h"
#include "views/controls/tabbed_pane/tabbed_pane.h"
#include "views/examples/example_base.h"

namespace examples {

// ButtonExample simply counts the number of clicks.
class ButtonExample : protected ExampleBase, private views::ButtonListener {
 public:
  ButtonExample(views::TabbedPane* tabbed_pane, views::Label* message)
      : ExampleBase(message),
        count_(0) {
    views::TextButton* button = new views::TextButton(this, L"Button");
    tabbed_pane->AddTab(L"Text Button", button);
  }

  virtual ~ButtonExample() {}

 private:
  // ButtonListner implementation.
  virtual void ButtonPressed(views::Button* sender, const views::Event& event) {
    PrintStatus(L"Pressed! count:%d", ++count_);
  }

  // The number of times the button is pressed.
  int count_;

  DISALLOW_COPY_AND_ASSIGN(ButtonExample);
};

}  // namespace examples

#endif  // VIEWS_EXAMPLES_BUTTON_EXAMPLE_H_

