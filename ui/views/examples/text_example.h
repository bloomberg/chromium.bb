// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_TEXT_EXAMPLE_H_
#define UI_VIEWS_EXAMPLES_TEXT_EXAMPLE_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/views/examples/example_base.h"
#include "views/controls/button/button.h"
#include "views/controls/combobox/combobox_listener.h"

namespace views {
class Checkbox;
class GridLayout;
}

namespace examples {

class TextExample : public ExampleBase,
                    public views::ButtonListener,
                    public views::ComboboxListener {
 public:
  explicit TextExample(ExamplesMain* main);
  virtual ~TextExample();

  // Overridden from ExampleBase:
  virtual void CreateExampleView(views::View* container) OVERRIDE;

 private:
  // Create and add a combo box to the layout.
  views::Combobox* AddCombobox(views::GridLayout* layout,
                               const char* name,
                               const char** strings,
                               int count);

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* button,
                             const views::Event& event) OVERRIDE;

  // Overridden from views::ComboboxListener:
  virtual void ItemChanged(views::Combobox* combo_box,
                           int prev_index,
                           int new_index) OVERRIDE;


  class TextExampleView;
  // The content of the scroll view.
  TextExampleView* text_view_;

  // Combo box for horizontal text alignment.
  views::Combobox* h_align_cb_;

  // Combo box for vertical text alignment.
  views::Combobox* v_align_cb_;

  // Combo box for text eliding style.
  views::Combobox* eliding_cb_;

  // Combo box for ampersand prefix show / hide behavior.
  views::Combobox* prefix_cb_;

  // Combo box to choose one of the sample texts.
  views::Combobox* text_cb_;

  // Check box to enable/disable multiline text drawing.
  views::Checkbox* multiline_checkbox_;

  // Check box to enable/disable character break behavior.
  views::Checkbox* break_checkbox_;

  DISALLOW_COPY_AND_ASSIGN(TextExample);
};

}  // namespace examples

#endif  // UI_VIEWS_EXAMPLES_TEXT_EXAMPLE_H_
