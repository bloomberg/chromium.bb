// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/radio_button_example.h"

#include <stddef.h>

#include <memory>

#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/radio_button.h"
#include "ui/views/examples/examples_window.h"
#include "ui/views/examples/grit/views_examples_resources.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/view.h"

using l10n_util::GetStringUTF16;
using l10n_util::GetStringUTF8;

namespace views {
namespace examples {

namespace {

const char* BoolToOnOff(bool value) {
  return value ? "on" : "off";
}

}  // namespace

RadioButtonExample::RadioButtonExample()
    : ExampleBase(GetStringUTF8(IDS_RADIO_BUTTON_SELECT_LABEL).c_str()) {}

RadioButtonExample::~RadioButtonExample() = default;

void RadioButtonExample::CreateExampleView(View* container) {
  GridLayout* layout =
      container->SetLayoutManager(std::make_unique<views::GridLayout>());
  ColumnSet* column_set = layout->AddColumnSet(0);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1.0f,
                        GridLayout::ColumnSize::kUsePreferred, 0, 0);
  const int group = 1;
  for (size_t i = 0; i < 3; ++i) {
    layout->StartRow(0, 0);
    radio_buttons_.push_back(layout->AddView(std::make_unique<RadioButton>(
        base::UTF8ToUTF16(base::StringPrintf("Radio %d in group %d",
                                             static_cast<int>(i) + 1, group)),
        group)));
  }

  layout->StartRow(0, 0);
  select_ = layout->AddView(std::make_unique<LabelButton>(
      this, GetStringUTF16(IDS_RADIO_BUTTON_SELECT_BUTTON_LABEL)));
  layout->StartRow(0, 0);
  status_ = layout->AddView(std::make_unique<LabelButton>(
      this, GetStringUTF16(IDS_RADIO_BUTTON_STATUS_LABEL)));
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
