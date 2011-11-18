// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/message_box_example.h"

#include "base/utf_string_conversions.h"
#include "ui/views/layout/grid_layout.h"
#include "views/controls/message_box_view.h"
#include "views/view.h"

namespace examples {

MessageBoxExample::MessageBoxExample(ExamplesMain* main)
    : ExampleBase(main, "Message Box View") {
}

MessageBoxExample::~MessageBoxExample() {
}

void MessageBoxExample::CreateExampleView(views::View* container) {
  message_box_view_ = new views::MessageBoxView(
      0,
      ASCIIToUTF16("Message Box Message"),
      ASCIIToUTF16("Default Prompt"));
  status_ = new views::TextButton(this, ASCIIToUTF16("Show Status"));
  toggle_ = new views::TextButton(this, ASCIIToUTF16("Toggle Checkbox"));

  views::GridLayout* layout = new views::GridLayout(container);
  container->SetLayoutManager(layout);

  message_box_view_->SetCheckBoxLabel(ASCIIToUTF16("Check Box"));

  const int message_box_column = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(message_box_column);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1,
                        views::GridLayout::USE_PREF, 0, 0);
  layout->StartRow(1 /* expand */, message_box_column);
  layout->AddView(message_box_view_);

  const int button_column = 1;
  column_set = layout->AddColumnSet(button_column);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL,
                        0.5f, views::GridLayout::USE_PREF, 0, 0);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL,
                        0.5f, views::GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0 /* no expand */, button_column);

  layout->AddView(status_);
  layout->AddView(toggle_);
}

void MessageBoxExample::ButtonPressed(views::Button* sender,
                                      const views::Event& event) {
  if (sender == status_) {
    message_box_view_->SetCheckBoxLabel(
        ASCIIToUTF16(BoolToOnOff(message_box_view_->IsCheckBoxSelected())));
    PrintStatus(message_box_view_->IsCheckBoxSelected() ?
       "Check Box Selected" : "Check Box Not Selected");
  } else if (sender == toggle_) {
    message_box_view_->SetCheckBoxSelected(
        !message_box_view_->IsCheckBoxSelected());
  }
}

}  // namespace examples
