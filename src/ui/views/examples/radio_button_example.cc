// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/radio_button_example.h"

#include <stddef.h>

#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/radio_button.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/view.h"

namespace views {
namespace examples {

namespace {

const char* BoolToOnOff(bool value) {
  return value ? "on" : "off";
}

}  // namespace

RadioButtonExample::RadioButtonExample() : ExampleBase("Radio Button") {}

RadioButtonExample::~RadioButtonExample() = default;

void RadioButtonExample::CreateExampleView(View* container) {
  GridLayout* layout = container->SetLayoutManager(
      std::make_unique<views::GridLayout>(container));
  ColumnSet* column_set = layout->AddColumnSet(0);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL,
                        1.0f, GridLayout::USE_PREF, 0, 0);

  for (size_t i = 0; i < 3; ++i) {
    layout->StartRow(0, 0);
    radio_buttons_.push_back(new RadioButton(
        base::ASCIIToUTF16("Radio ") + base::NumberToString16(i), 1));
    layout->AddView(radio_buttons_.back());
  }

  layout->StartRow(0, 0);
  select_ = new LabelButton(this, base::ASCIIToUTF16("Select"));
  layout->AddView(select_);

  layout->StartRow(0, 0);
  status_ = new LabelButton(this, base::ASCIIToUTF16("Show Status"));
  layout->AddView(status_);
}

void RadioButtonExample::ButtonPressed(Button* sender, const ui::Event& event) {
  if (sender == select_) {
    radio_buttons_[2]->SetChecked(true);
  } else if (sender == status_) {
    // Show the state of radio buttons.
    PrintStatus("Group: 1:%s, 2:%s, 3:%s",
                BoolToOnOff(radio_buttons_[0]->GetChecked()),
                BoolToOnOff(radio_buttons_[1]->GetChecked()),
                BoolToOnOff(radio_buttons_[2]->GetChecked()));
  }
}

}  // namespace examples
}  // namespace views
