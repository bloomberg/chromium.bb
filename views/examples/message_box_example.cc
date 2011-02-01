// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/examples/message_box_example.h"

#include "views/layout/grid_layout.h"
#include "views/view.h"

namespace examples {

MessageBoxExample::MessageBoxExample(ExamplesMain* main)
    : ExampleBase(main) {
}

MessageBoxExample::~MessageBoxExample() {
}

std::wstring MessageBoxExample::GetExampleTitle() {
  return L"Message Box View";
}

void MessageBoxExample::CreateExampleView(views::View* container) {
  message_box_view_ = new MessageBoxView(
      0, L"Message Box Message", L"Default Prompt");
  status_ = new views::TextButton(this, L"Show Status");
  toggle_ = new views::TextButton(this, L"Toggle Checkbox");

  views::GridLayout* layout = new views::GridLayout(container);
  container->SetLayoutManager(layout);

  message_box_view_->SetCheckBoxLabel(L"Check Box");

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
        IntToOnOff(message_box_view_->IsCheckBoxSelected()));
    PrintStatus(message_box_view_->IsCheckBoxSelected() ?
       L"Check Box Selected"  : L"Check Box Not Selected");
  } else if (sender == toggle_) {
    message_box_view_->SetCheckBoxSelected(
        !message_box_view_->IsCheckBoxSelected());
  }
}

}  // namespace examples
