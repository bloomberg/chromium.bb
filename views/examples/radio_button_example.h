// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_EXAMPLES_RADIO_BUTTON_EXAMPLE_H_
#define VIEWS_EXAMPLES_RADIO_BUTTON_EXAMPLE_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "views/controls/button/radio_button.h"
#include "views/controls/button/text_button.h"
#include "views/examples/example_base.h"

namespace examples {

class RadioButtonExample : public ExampleBase,
                           public views::ButtonListener {
 public:
  explicit RadioButtonExample(ExamplesMain* main);
  virtual ~RadioButtonExample();

  // Overridden from ExampleBase:
  virtual std::wstring GetExampleTitle() OVERRIDE;
  virtual void CreateExampleView(views::View* container) OVERRIDE;

 private:
  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event) OVERRIDE;

  // 6 radio buttons, 0-2 consists 1st group, and 3-5 consists
  // 2nd group.
  views::RadioButton* radio_buttons_[6];

  // Control button to select radio buttons, and show the status of buttons.
  views::TextButton* select_;
  views::TextButton* status_;

  DISALLOW_COPY_AND_ASSIGN(RadioButtonExample);
};

}  // namespace examples

#endif  // VIEWS_EXAMPLES_RADIO_BUTTON_EXAMPLE_H_
