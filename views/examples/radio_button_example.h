// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_EXAMPLES_RADIO_BUTTON_EXAMPLE_H_
#define VIEWS_EXAMPLES_RADIO_BUTTON_EXAMPLE_H_

#include "base/compiler_specific.h"
#include "base/string_util.h"
#include "views/controls/button/radio_button.h"
#include "views/controls/button/text_button.h"
#include "views/examples/example_base.h"

namespace examples {

class RadioButtonExample : protected ExampleBase,
                           private views::ButtonListener {
 public:
  explicit RadioButtonExample(ExamplesMain* main) : ExampleBase(main) {
    select_ = new views::TextButton(this, L"Select");
    status_ = new views::TextButton(this, L"Show Status");

    int all = arraysize(radio_buttons_);

    // divide buttons into 2 groups
    int group_count = all / 2;
    for (int i = 0; i < all; i++) {
      int group = i / group_count;
      radio_buttons_[i] = new views::RadioButton(
          StringPrintf(L"Radio %d in group %d", (i % group_count + 1), group),
          group);
    }

    container_ = new views::View();
    views::GridLayout* layout = new views::GridLayout(container_);
    container_->SetLayoutManager(layout);

    views::ColumnSet* column_set = layout->AddColumnSet(0);
    column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL,
                          1.0f, views::GridLayout::USE_PREF, 0, 0);
    for (int i = 0; i < all; i++) {
      layout->StartRow(0, 0);
      layout->AddView(radio_buttons_[i]);
    }
    layout->StartRow(0, 0);
    layout->AddView(select_);
    layout->StartRow(0, 0);
    layout->AddView(status_);
  }

  virtual ~RadioButtonExample() {}

  virtual std::wstring GetExampleTitle() {
    return L"Radio Button";
  }

  virtual views::View* GetExampleView() {
    return container_;
  }

 private:
  // Override from ButtonListener
  virtual void ButtonPressed(views::Button* sender, const views::Event& event) {
    if (sender == select_) {
      radio_buttons_[0]->SetChecked(true);
      radio_buttons_[5]->SetChecked(true);
    } else if (sender == status_) {
      // Show the state of radio buttons.
      PrintStatus(L"Group1: 1:%ls, 2:%ls, 3:%ls   Group2: 1:%ls, 2:%ls, 3:%ls",
                  IntToOnOff(radio_buttons_[0]->checked()),
                  IntToOnOff(radio_buttons_[1]->checked()),
                  IntToOnOff(radio_buttons_[2]->checked()),
                  IntToOnOff(radio_buttons_[3]->checked()),
                  IntToOnOff(radio_buttons_[4]->checked()),
                  IntToOnOff(radio_buttons_[5]->checked()));
    }
  }

  // The view containing this test's controls.
  views::View* container_;

  // 6 radio buttons, 0-2 consists 1st group, and 3-5 consists
  // 2nd group.
  views::RadioButton* radio_buttons_[6];

  // Control button to select radio buttons, and show the status of buttons.
  views::TextButton* select_, *status_;

  DISALLOW_COPY_AND_ASSIGN(RadioButtonExample);
};

}  // namespace examples

#endif  // VIEWS_EXAMPLES_RADIO_BUTTON_EXAMPLE_H_

