// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_EXAMPLES_MESSAGE_BOX_EXAMPLE_H_
#define VIEWS_EXAMPLES_MESSAGE_BOX_EXAMPLE_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/string_util.h"
#include "views/controls/button/text_button.h"
#include "views/controls/message_box_view.h"
#include "views/controls/tabbed_pane/tabbed_pane.h"
#include "views/examples/example_base.h"
#include "views/layout/grid_layout.h"

namespace examples {

// A MessageBoxView example. This tests some of checkbox features as well.
class MessageBoxExample : public ExampleBase,
                          public views::ButtonListener {
 public:
  explicit MessageBoxExample(ExamplesMain* main) : ExampleBase(main) {}

  virtual ~MessageBoxExample() {}

  virtual std::wstring GetExampleTitle() {
    return L"Message Box View";
  }

  virtual void CreateExampleView(views::View* container) {
    message_box_view_ =
        new MessageBoxView(0, L"Message Box Message", L"Default Prompt");
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

 private:
  // ButtonListener overrides.
  virtual void ButtonPressed(views::Button* sender, const views::Event& event) {
    if (sender == status_) {
      message_box_view_->SetCheckBoxLabel(
          IntToOnOff(message_box_view_->IsCheckBoxSelected()));
      PrintStatus(message_box_view_->IsCheckBoxSelected() ?
                   L"Check Box Selected" : L"Check Box Not Selected");
    } else if (sender == toggle_) {
      message_box_view_->SetCheckBoxSelected(
          !message_box_view_->IsCheckBoxSelected());
    }
  }

  // The MessageBoxView to be tested.
  MessageBoxView* message_box_view_;

  // Control buttons to show the status and toggle checkbox in the
  // message box.
  views::Button* status_, *toggle_;

  DISALLOW_COPY_AND_ASSIGN(MessageBoxExample);
};

}  // namespace examples

#endif  // VIEWS_EXAMPLES_MESSAGE_BOX_EXAMPLE_H_

