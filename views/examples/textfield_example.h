// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_EXAMPLES_TEXTFIELD_EXAMPLE_H_
#define VIEWS_EXAMPLES_TEXTFIELD_EXAMPLE_H_

#include "base/string_util.h"
#include "views/controls/label.h"
#include "views/controls/tabbed_pane/tabbed_pane.h"
#include "views/controls/textfield/textfield.h"
#include "views/examples/example_base.h"

namespace examples {

using views::Textfield;

// TextfieldExample mimics login screen.
class TextfieldExample : protected ExampleBase,
                         public Textfield::Controller {
 public:
  TextfieldExample(views::TabbedPane* tabbed_pane, views::Label* message)
      : ExampleBase(message),
        name_(new Textfield()),
        password_(new Textfield(Textfield::STYLE_PASSWORD)) {
    name_->SetController(this);
    password_->SetController(this);

    views::View* container = new views::View();
    tabbed_pane->AddTab(L"Textfield", container);
    views::GridLayout* layout = new views::GridLayout(container);
    container->SetLayoutManager(layout);

    views::ColumnSet* column_set = layout->AddColumnSet(0);
    column_set->AddColumn(views::GridLayout::LEADING, views::GridLayout::FILL,
                          0.2f, views::GridLayout::USE_PREF, 0, 0);
    column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL,
                          0.8f, views::GridLayout::USE_PREF, 0, 0);
    layout->StartRow(0, 0);
    layout->AddView(new views::Label(L"Name:"));
    layout->AddView(name_);
    layout->StartRow(0, 0);
    layout->AddView(new views::Label(L"Password:"));
    layout->AddView(password_);
  }

  virtual ~TextfieldExample() {}

 private:
  // Textfield::Controller implementations:
  // This method is called whenever the text in the field changes.
  virtual void ContentsChanged(Textfield* sender,
                               const string16& new_contents) {
    if (sender == name_) {
      PrintStatus(L"Name [%ls]", UTF16ToWideHack(new_contents).c_str());
    } else if (sender == password_) {
      PrintStatus(L"Password [%ls]", UTF16ToWideHack(new_contents).c_str());
    }
  }

  // Let the control handle keystrokes.
  virtual bool HandleKeystroke(Textfield* sender,
                               const Textfield::Keystroke& keystroke) {
    return false;
  }

  // Textfields for name and password.
  views::Textfield* name_, *password_;

  DISALLOW_COPY_AND_ASSIGN(TextfieldExample);
};

}  // namespace examples

#endif  // VIEWS_EXAMPLES_TEXTFIELD_EXAMPLE_H_

