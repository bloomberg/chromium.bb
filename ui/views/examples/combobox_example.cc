// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/combobox_example.h"

#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/layout/fill_layout.h"

namespace views {
namespace examples {

ComboboxModelExample::ComboboxModelExample() {
}

ComboboxModelExample::~ComboboxModelExample() {
}

int ComboboxModelExample::GetItemCount() const {
  return 10;
}

string16 ComboboxModelExample::GetItemAt(int index) {
  return UTF8ToUTF16(base::StringPrintf("Item %d", index));
}

ComboboxExample::ComboboxExample() : ExampleBase("Combo Box"), combobox_(NULL) {
}

ComboboxExample::~ComboboxExample() {
}

void ComboboxExample::CreateExampleView(View* container) {
  combobox_ = new Combobox(&combobox_model_);
  combobox_->set_listener(this);
  combobox_->SetSelectedItem(3);

  container->SetLayoutManager(new FillLayout);
  container->AddChildView(combobox_);
}

void ComboboxExample::ItemChanged(Combobox* combo_box,
                                  int prev_index,
                                  int new_index) {
  DCHECK_EQ(combobox_, combo_box);
  PrintStatus("Selected: %s",
              UTF16ToUTF8(combobox_model_.GetItemAt(new_index)).c_str());
}

}  // namespace examples
}  // namespace views
