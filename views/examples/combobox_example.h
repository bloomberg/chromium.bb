// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_EXAMPLES_COMBOBOX_EXAMPLE_H_
#define VIEWS_EXAMPLES_COMBOBOX_EXAMPLE_H_

#include "app/combobox_model.h"
#include "base/string_util.h"
#include "views/controls/combobox/combobox.h"
#include "views/examples/example_base.h"

namespace examples {

// ComboboxExample
class ComboboxExample : protected ExampleBase,
                        private views::Combobox::Listener {
 public:
  ComboboxExample(views::TabbedPane* tabbed_pane, views::Label* message)
      : ExampleBase(message) {
    views::Combobox* cb = new views::Combobox(new ComboboxModelExample());
    cb->set_listener(this);
    tabbed_pane->AddTab(L"Combo Box", cb);
  }
  virtual ~ComboboxExample() {}

 private:
  // An sample combobox model that generates list of "Item <index>".
  class ComboboxModelExample : public ComboboxModel {
   public:
    ComboboxModelExample() {}
    virtual ~ComboboxModelExample() {}

    virtual int GetItemCount() {
      return 10;
    }

    virtual std::wstring GetItemAt(int index) {
      return StringPrintf(L"Item %d", index);
    }

   private:
    DISALLOW_COPY_AND_ASSIGN(ComboboxModelExample);
  };

  // Lister implementation.
  virtual void ItemChanged(views::Combobox* combo_box,
                           int prev_index,
                           int new_index) {
    PrintStatus(L"Selected: index=%d, label=%ls",
                new_index, combo_box->model()->GetItemAt(new_index).c_str());
  }

  DISALLOW_COPY_AND_ASSIGN(ComboboxExample);
};

}  // namespace examples

#endif  // VIEWS_EXAMPLES_COMBOBOX_EXAMPLE_H_


