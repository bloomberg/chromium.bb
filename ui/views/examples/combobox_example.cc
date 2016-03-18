// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/combobox_example.h"

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/layout/box_layout.h"

namespace views {
namespace examples {

ComboboxModelExample::ComboboxModelExample() {
}

ComboboxModelExample::~ComboboxModelExample() {
}

int ComboboxModelExample::GetItemCount() const {
  return 10;
}

base::string16 ComboboxModelExample::GetItemAt(int index) {
  return base::UTF8ToUTF16(base::StringPrintf("Item %d", index));
}

ComboboxExample::ComboboxExample() : ExampleBase("Combo Box") {
}

ComboboxExample::~ComboboxExample() {
  // Delete |combobox_| first as it references |combobox_model_|.
  delete combobox_;
  delete action_combobox_;
  combobox_ = nullptr;
  action_combobox_ = nullptr;
}

void ComboboxExample::CreateExampleView(View* container) {
  combobox_ = new Combobox(&combobox_model_);
  combobox_->set_listener(this);
  combobox_->SetSelectedIndex(3);

  action_combobox_ = new Combobox(&combobox_model_);
  action_combobox_->set_listener(this);
  action_combobox_->SetStyle(Combobox::STYLE_ACTION);
  // Note: STYLE_ACTION comboboxes always have the first item selected by
  // default.

  container->SetLayoutManager(new BoxLayout(
      BoxLayout::kVertical,
      1, 1, 1));
  container->AddChildView(combobox_);
  container->AddChildView(action_combobox_);
}

void ComboboxExample::OnPerformAction(Combobox* combobox) {
  if (combobox == combobox_) {
    PrintStatus("Selected: %s", base::UTF16ToUTF8(combobox_model_.GetItemAt(
        combobox->selected_index())).c_str());
  } else if (combobox == action_combobox_) {
    PrintStatus("Action: %s", base::UTF16ToUTF8(combobox_model_.GetItemAt(
        combobox->selected_index())).c_str());
  } else {
    NOTREACHED() << "Surprising combobox.";
  }
}

}  // namespace examples
}  // namespace views
