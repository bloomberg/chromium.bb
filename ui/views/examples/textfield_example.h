// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_TEXTFIELD_EXAMPLE_H_
#define UI_VIEWS_EXAMPLES_TEXTFIELD_EXAMPLE_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/string16.h"
#include "ui/views/controls/button/text_button.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/examples/example_base.h"

namespace views {
namespace examples {

// TextfieldExample mimics login screen.
class TextfieldExample : public ExampleBase,
                         public views::TextfieldController,
                         public views::ButtonListener {
 public:
  TextfieldExample();
  virtual ~TextfieldExample();

  // ExampleBase:
  virtual void CreateExampleView(View* container) OVERRIDE;

 private:
  // TextfieldController:
  virtual void ContentsChanged(Textfield* sender,
                               const string16& new_contents) OVERRIDE;
  virtual bool HandleKeyEvent(Textfield* sender,
                              const KeyEvent& key_event) OVERRIDE;

  // ButtonListener:
  virtual void ButtonPressed(Button* sender, const Event& event) OVERRIDE;

  // Textfields for name and password.
  Textfield* name_;
  Textfield* password_;

  // Various buttons to control textfield.
  TextButton* show_password_;
  TextButton* clear_all_;
  TextButton* append_;
  TextButton* set_;
  TextButton* set_style_;

  DISALLOW_COPY_AND_ASSIGN(TextfieldExample);
};

}  // namespace examples
}  // namespace views

#endif  // UI_VIEWS_EXAMPLES_TEXTFIELD_EXAMPLE_H_
