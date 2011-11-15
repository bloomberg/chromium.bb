// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_COMBOBOX_EXAMPLE_H_
#define UI_VIEWS_EXAMPLES_COMBOBOX_EXAMPLE_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/views/examples/example_base.h"
#include "views/controls/combobox/combobox.h"
#include "views/layout/fill_layout.h"

namespace examples {

class ComboboxExample : public ExampleBase,
                        public views::Combobox::Listener {
 public:
  explicit ComboboxExample(ExamplesMain* main);
  virtual ~ComboboxExample();

  // Overridden from ExampleBase:
  virtual void CreateExampleView(views::View* container) OVERRIDE;

 private:
  // Overridden from views::Combobox::Listener:
  virtual void ItemChanged(views::Combobox* combo_box,
                           int prev_index,
                           int new_index) OVERRIDE;

  // This test only control.
  views::Combobox* combobox_;

  DISALLOW_COPY_AND_ASSIGN(ComboboxExample);
};

}  // namespace examples

#endif  // UI_VIEWS_EXAMPLES_COMBOBOX_EXAMPLE_H_
