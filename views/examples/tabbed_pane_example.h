// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_EXAMPLES_TABBED_PANE_EXAMPLE_H_
#define VIEWS_EXAMPLES_TABBED_PANE_EXAMPLE_H_

#include "base/string_util.h"
#include "views/controls/button/text_button.h"
#include "views/examples/example_base.h"

namespace examples {

// A TabbedPane example tests adding/removing/selecting tabs.
class TabbedPaneExample : public ExampleBase,
                          public views::ButtonListener,
                          views::TabbedPane::Listener {
 public:
  explicit TabbedPaneExample(ExamplesMain* main) : ExampleBase(main) {}

  virtual ~TabbedPaneExample() {}

  virtual std::wstring GetExampleTitle() {
    return L"Tabbed Pane";
  }

  virtual void CreateExampleView(views::View* container) {
    tabbed_pane_ = new views::TabbedPane();
    add_ = new views::TextButton(this, L"Add");
    add_at_ = new views::TextButton(this, L"Add At 1");
    remove_at_ = new views::TextButton(this, L"Remove At 1");
    select_at_ = new views::TextButton(this, L"Select At 1");

    views::GridLayout* layout = new views::GridLayout(container);
    container->SetLayoutManager(layout);

    const int tabbed_pane_column = 0;
    views::ColumnSet* column_set = layout->AddColumnSet(tabbed_pane_column);
    column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL,
                          1.0f, views::GridLayout::USE_PREF, 0, 0);
    layout->StartRow(1 /* expand */, tabbed_pane_column);
    layout->AddView(tabbed_pane_);

    // Create a few tabs with a button first.
    AddButton(L"Tab 1");
    AddButton(L"Tab 2");

    // Add control buttons horizontally.
    const int button_column = 1;
    column_set = layout->AddColumnSet(button_column);
    for (int i = 0; i < 4; i++) {
      column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL,
                            1.0f, views::GridLayout::USE_PREF, 0, 0);
    }

    layout->StartRow(0 /* no expand */, button_column);
    layout->AddView(add_);
    layout->AddView(add_at_);
    layout->AddView(remove_at_);
    layout->AddView(select_at_);
  }

 private:
  // ButtonListener overrides.
  virtual void ButtonPressed(views::Button* sender, const views::Event& event) {
    if (sender == add_) {
      AddButton(L"Added");
    } else if (sender == add_at_) {
      const std::wstring label = L"Added at 1";
      tabbed_pane_->AddTabAtIndex(1, label,
                                  new views::TextButton(NULL, label), true);
    } else if (sender == remove_at_) {
      if (tabbed_pane_->GetTabCount() > 1)
        delete tabbed_pane_->RemoveTabAtIndex(1);
    } else if (sender == select_at_) {
      if (tabbed_pane_->GetTabCount() > 1)
        tabbed_pane_->SelectTabAt(1);
    }
    PrintStatus();
  }

  // TabbedPane::Listener overrides.
  virtual void TabSelectedAt(int index) {
    // Just print the status when selection changes.
    PrintStatus();
  }

  // Print the status of the tab in the status area.
  void PrintStatus() {
    ExampleBase::PrintStatus(L"Tab Count:%d, Selected Tab:%d",
                             tabbed_pane_->GetTabCount(),
                             tabbed_pane_->GetSelectedTabIndex());
  }

  void AddButton(const std::wstring& label) {
    views::TextButton* button = new views::TextButton(NULL, label);
    tabbed_pane_->AddTab(label, button);
  }

  // The tabbed pane to be tested.
  views::TabbedPane* tabbed_pane_;

  // Control buttons to add, remove or select tabs.
  views::Button* add_, *add_at_, *remove_at_, *select_at_;

  DISALLOW_COPY_AND_ASSIGN(TabbedPaneExample);
};

}  // namespace examples

#endif  // VIEWS_EXAMPLES_TABBED_PANE_EXAMPLE_H_

