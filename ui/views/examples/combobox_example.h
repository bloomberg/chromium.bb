// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_COMBOBOX_EXAMPLE_H_
#define UI_VIEWS_EXAMPLES_COMBOBOX_EXAMPLE_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/views/controls/combobox/combobox_listener.h"
#include "ui/views/examples/example_base.h"

namespace views {
namespace examples {

class ComboboxExample : public ExampleBase, public ComboboxListener {
 public:
  ComboboxExample();
  virtual ~ComboboxExample();

  // Overridden from ExampleBase:
  virtual void CreateExampleView(View* container) OVERRIDE;

 private:
  // Overridden from ComboboxListener:
  virtual void ItemChanged(Combobox* combo_box,
                           int prev_index,
                           int new_index) OVERRIDE;

  Combobox* combobox_;

  DISALLOW_COPY_AND_ASSIGN(ComboboxExample);
};

}  // namespace examples
}  // namespace views

#endif  // UI_VIEWS_EXAMPLES_COMBOBOX_EXAMPLE_H_
