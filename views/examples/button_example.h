// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_EXAMPLES_BUTTON_EXAMPLE_H_
#define VIEWS_EXAMPLES_BUTTON_EXAMPLE_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "views/controls/button/text_button.h"
#include "views/examples/example_base.h"

namespace views {
class View;
}

namespace examples {

// ButtonExample simply counts the number of clicks.
class ButtonExample : public ExampleBase, public views::ButtonListener {
 public:
  explicit ButtonExample(ExamplesMain* main);
  virtual ~ButtonExample();

  // Overridden from ExampleBase:
  virtual std::wstring GetExampleTitle() OVERRIDE;
  virtual void CreateExampleView(views::View* container) OVERRIDE;

 private:
  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event) OVERRIDE;

  // The only control in this test.
  views::TextButton* button_;

  // The number of times the button is pressed.
  int count_;

  DISALLOW_COPY_AND_ASSIGN(ButtonExample);
};

}  // namespace examples

#endif  // VIEWS_EXAMPLES_BUTTON_EXAMPLE_H_
