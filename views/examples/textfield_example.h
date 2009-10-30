// Copyight (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_EXAMPLES_TEXTFIELD_EXAMPLE_H_
#define VIEWS_EXAMPLES_TEXTFIELD_EXAMPLE_H_

#include "base/string_util.h"
#include "views/controls/button/text_button.h"
#include "views/controls/label.h"
#include "views/controls/tabbed_pane/tabbed_pane.h"
#include "views/controls/textfield/textfield.h"
#include "views/examples/example_base.h"

namespace examples {

using views::Textfield;

// TextfieldExample mimics login screen.
class TextfieldExample : public ExampleBase,
                         public Textfield::Controller,
                         public views::ButtonListener {
 public:
  explicit TextfieldExample(ExamplesMain* main) : ExampleBase(main) {}

  virtual ~TextfieldExample() {}

  virtual std::wstring GetExampleTitle() {
    return L"Textfield";
  }

  virtual void CreateExampleView(views::View* container) {
    name_ = new Textfield();
    password_ = new Textfield(Textfield::STYLE_PASSWORD);
    show_password_ = new views::TextButton(this, L"Show password");
    clear_all_ = new views::TextButton(this, L"Clear All");
    append_ = new views::TextButton(this, L"Append");
    name_->SetController(this);
    password_->SetController(this);

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
    layout->StartRow(0, 0);
    layout->AddView(show_password_);
    layout->StartRow(0, 0);
    layout->AddView(clear_all_);
    layout->StartRow(0, 0);
    layout->AddView(append_);
  }

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

  // ButtonListner implementation.
  virtual void ButtonPressed(views::Button* sender, const views::Event& event) {
    if (sender == show_password_) {
      PrintStatus(L"Password [%ls]",
                  UTF16ToWideHack(password_->text()).c_str());
    } else if (sender == clear_all_) {
      string16 empty;
      name_->SetText(empty);
      password_->SetText(empty);
    } else if (sender == append_) {
      name_->AppendText(WideToUTF16(L"[append]"));
    }
  }

  // Textfields for name and password.
  views::Textfield* name_;
  views::Textfield* password_;

  // Buttons to show password text and to clear the textfields.
  views::TextButton* show_password_;
  views::TextButton* clear_all_;
  views::TextButton* append_;

  DISALLOW_COPY_AND_ASSIGN(TextfieldExample);
};

}  // namespace examples

#endif  // VIEWS_EXAMPLES_TEXTFIELD_EXAMPLE_H_

