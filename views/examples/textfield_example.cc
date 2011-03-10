// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/examples/textfield_example.h"

#include "base/utf_string_conversions.h"
#include "views/controls/label.h"
#include "views/controls/textfield/textfield.h"
#include "views/layout/grid_layout.h"
#include "views/view.h"

namespace examples {

TextfieldExample::TextfieldExample(ExamplesMain* main)
    : ExampleBase(main) {
}

TextfieldExample::~TextfieldExample() {
}

std::wstring TextfieldExample::GetExampleTitle() {
  return L"Textfield";
}

void TextfieldExample::CreateExampleView(views::View* container) {
  name_ = new views::Textfield();
  password_ = new views::Textfield(views::Textfield::STYLE_PASSWORD);
  password_->set_text_to_display_when_empty(ASCIIToUTF16("password"));
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

void TextfieldExample::ContentsChanged(views::Textfield* sender,
                                       const string16& new_contents) {
  if (sender == name_) {
    PrintStatus(L"Name [%ls]", UTF16ToWideHack(new_contents).c_str());
  } else if (sender == password_) {
    PrintStatus(L"Password [%ls]", UTF16ToWideHack(new_contents).c_str());
  }
}

bool TextfieldExample::HandleKeyEvent(views::Textfield* sender,
                                      const views::KeyEvent& key_event) {
  return false;
}

void TextfieldExample::ButtonPressed(views::Button* sender,
                                     const views::Event& event) {
  if (sender == show_password_) {
    PrintStatus(L"Password [%ls]", UTF16ToWideHack(password_->text()).c_str());
  } else if (sender == clear_all_) {
    string16 empty;
    name_->SetText(empty);
    password_->SetText(empty);
  } else if (sender == append_) {
    name_->AppendText(WideToUTF16(L"[append]"));
  }
}

}  // namespace examples
