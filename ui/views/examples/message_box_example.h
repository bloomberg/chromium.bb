// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_MESSAGE_BOX_EXAMPLE_H_
#define UI_VIEWS_EXAMPLES_MESSAGE_BOX_EXAMPLE_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/views/controls/button/text_button.h"
#include "ui/views/examples/example_base.h"

namespace views {
class MessageBoxView;
}

namespace examples {

// A MessageBoxView example. This tests some of checkbox features as well.
class MessageBoxExample : public ExampleBase,
                          public views::ButtonListener {
 public:
  explicit MessageBoxExample(ExamplesMain* main);
  virtual ~MessageBoxExample();

  // Overridden from ExampleBase:
  virtual void CreateExampleView(views::View* container) OVERRIDE;

 private:
  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event) OVERRIDE;

  // The MessageBoxView to be tested.
  views::MessageBoxView* message_box_view_;

  // Control buttons to show the status and toggle checkbox in the
  // message box.
  views::Button* status_;
  views::Button* toggle_;

  DISALLOW_COPY_AND_ASSIGN(MessageBoxExample);
};

}  // namespace examples

#endif  // UI_VIEWS_EXAMPLES_MESSAGE_BOX_EXAMPLE_H_
