// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_MULTILINE_EXAMPLE_H_
#define UI_VIEWS_EXAMPLES_MULTILINE_EXAMPLE_H_

#include "base/compiler_specific.h"
#include "base/strings/string16.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/examples/example_base.h"

namespace views {

class Label;

namespace examples {

// An example that compares the multi-line rendering of different controls.
class MultilineExample : public ExampleBase,
                         public TextfieldController {
 public:
  MultilineExample();
  virtual ~MultilineExample();

  // ExampleBase:
  virtual void CreateExampleView(View* container) OVERRIDE;

 private:
  class RenderTextView;

  // TextfieldController:
  virtual void ContentsChanged(Textfield* sender,
                               const string16& new_contents) OVERRIDE;
  virtual bool HandleKeyEvent(Textfield* sender,
                              const ui::KeyEvent& key_event) OVERRIDE;

  RenderTextView* render_text_view_;
  Label* label_;
  Textfield* textfield_;

  DISALLOW_COPY_AND_ASSIGN(MultilineExample);
};

}  // namespace examples
}  // namespace views

#endif  // UI_VIEWS_EXAMPLES_MULTILINE_EXAMPLE_H_
