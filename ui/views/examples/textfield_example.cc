// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/textfield_example.h"

#include "base/utf_string_conversions.h"
#include "ui/base/range/range.h"
#include "ui/gfx/render_text.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/view.h"

namespace views {
namespace examples {

TextfieldExample::TextfieldExample() : ExampleBase("Textfield") {
}

TextfieldExample::~TextfieldExample() {
}

void TextfieldExample::CreateExampleView(View* container) {
  name_ = new Textfield();
  password_ = new Textfield(Textfield::STYLE_PASSWORD);
  password_->set_text_to_display_when_empty(ASCIIToUTF16("password"));
  show_password_ = new TextButton(this, ASCIIToUTF16("Show password"));
  clear_all_ = new TextButton(this, ASCIIToUTF16("Clear All"));
  append_ = new TextButton(this, ASCIIToUTF16("Append"));
  set_ = new TextButton(this, ASCIIToUTF16("Set"));
  set_style_ = new TextButton(this, ASCIIToUTF16("Set Styles"));
  name_->SetController(this);
  password_->SetController(this);

  GridLayout* layout = new GridLayout(container);
  container->SetLayoutManager(layout);

  ColumnSet* column_set = layout->AddColumnSet(0);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::FILL,
                        0.2f, GridLayout::USE_PREF, 0, 0);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL,
                        0.8f, GridLayout::USE_PREF, 0, 0);
  layout->StartRow(0, 0);
  layout->AddView(new Label(ASCIIToUTF16("Name:")));
  layout->AddView(name_);
  layout->StartRow(0, 0);
  layout->AddView(new Label(ASCIIToUTF16("Password:")));
  layout->AddView(password_);
  layout->StartRow(0, 0);
  layout->AddView(show_password_);
  layout->StartRow(0, 0);
  layout->AddView(clear_all_);
  layout->StartRow(0, 0);
  layout->AddView(append_);
  layout->StartRow(0, 0);
  layout->AddView(set_);
  layout->StartRow(0, 0);
  layout->AddView(set_style_);
}

void TextfieldExample::ContentsChanged(Textfield* sender,
                                       const string16& new_contents) {
  if (sender == name_) {
    PrintStatus("Name [%s]", UTF16ToUTF8(new_contents).c_str());
  } else if (sender == password_) {
    PrintStatus("Password [%s]", UTF16ToUTF8(new_contents).c_str());
  }
}

bool TextfieldExample::HandleKeyEvent(Textfield* sender,
                                      const KeyEvent& key_event) {
  return false;
}

void TextfieldExample::ButtonPressed(Button* sender,
                                     const Event& event) {
  if (sender == show_password_) {
    PrintStatus("Password [%s]", UTF16ToUTF8(password_->text()).c_str());
  } else if (sender == clear_all_) {
    string16 empty;
    name_->SetText(empty);
    password_->SetText(empty);
  } else if (sender == append_) {
    name_->AppendText(WideToUTF16(L"[append]"));
  } else if (sender == set_) {
    name_->SetText(WideToUTF16(L"[set]"));
  } else if (sender == set_style_) {
    if (!name_->text().empty()) {
      gfx::StyleRange color;
      color.foreground = SK_ColorYELLOW;
      color.range = ui::Range(0, name_->text().length());
      name_->ApplyStyleRange(color);

      if (name_->text().length() >= 5) {
        size_t fifth = name_->text().length() / 5;
        gfx::StyleRange underline;
        underline.underline = true;
        underline.foreground = SK_ColorBLUE;
        underline.range = ui::Range(1 * fifth, 4 * fifth);
        name_->ApplyStyleRange(underline);

        gfx::StyleRange strike;
        strike.strike = true;
        strike.foreground = SK_ColorRED;
        strike.range = ui::Range(2 * fifth, 3 * fifth);
        name_->ApplyStyleRange(strike);
      }
    }
  }
}

}  // namespace examples
}  // namespace views
