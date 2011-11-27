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
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/examples/example_base.h"
#include "views/controls/button/text_button.h"

namespace examples {

// TextfieldExample mimics login screen.
class TextfieldExample : public ExampleBase,
                         public views::TextfieldController,
                         public views::ButtonListener {
 public:
  explicit TextfieldExample(ExamplesMain* main);
  virtual ~TextfieldExample();

  // ExampleBase:
  virtual void CreateExampleView(views::View* container) OVERRIDE;

 private:
  // views::TextfieldController:
  virtual void ContentsChanged(views::Textfield* sender,
                               const string16& new_contents) OVERRIDE;
  virtual bool HandleKeyEvent(views::Textfield* sender,
                              const views::KeyEvent& key_event) OVERRIDE;

  // views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event) OVERRIDE;

  // Textfields for name and password.
  views::Textfield* name_;
  views::Textfield* password_;

  // Various buttons to control textfield.
  views::TextButton* show_password_;
  views::TextButton* clear_all_;
  views::TextButton* append_;
  views::TextButton* set_;
  views::TextButton* set_style_;

  DISALLOW_COPY_AND_ASSIGN(TextfieldExample);
};

}  // namespace examples

#endif  // UI_VIEWS_EXAMPLES_TEXTFIELD_EXAMPLE_H_
