// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/combobox_example.h"

#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "ui/base/models/combobox_model.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/layout/fill_layout.h"

namespace {

// An sample combobox model that generates list of "Item <index>".
class ComboboxModelExample : public ui::ComboboxModel {
 public:
  ComboboxModelExample() {}
  virtual ~ComboboxModelExample() {}

  // Overridden from ui::ComboboxModel:
  virtual int GetItemCount() OVERRIDE { return 10; }
  virtual string16 GetItemAt(int index) OVERRIDE {
    return UTF8ToUTF16(base::StringPrintf("Item %d", index));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ComboboxModelExample);
};

}  // namespace

namespace views {
namespace examples {

ComboboxExample::ComboboxExample() : ExampleBase("Combo Box") {
}

ComboboxExample::~ComboboxExample() {
}

void ComboboxExample::CreateExampleView(View* container) {
  combobox_ = new Combobox(new ComboboxModelExample());
  combobox_->set_listener(this);
  combobox_->SetSelectedItem(3);

  container->SetLayoutManager(new FillLayout);
  container->AddChildView(combobox_);
}

void ComboboxExample::ItemChanged(Combobox* combo_box,
                                  int prev_index,
                                  int new_index) {
  PrintStatus("Selected: index=%d, label=%s",
      new_index, UTF16ToUTF8(combo_box->model()->GetItemAt(new_index)).c_str());
}

}  // namespace examples
}  // namespace views
