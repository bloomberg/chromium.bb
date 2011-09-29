// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/examples/tabbed_pane_example.h"

#include "views/controls/tabbed_pane/tabbed_pane.h"
#include "views/layout/grid_layout.h"

namespace examples {

TabbedPaneExample::TabbedPaneExample(ExamplesMain* main)
    : ExampleBase(main, "Tabbed Pane") {
}

TabbedPaneExample::~TabbedPaneExample() {
}

void TabbedPaneExample::CreateExampleView(views::View* container) {
  tabbed_pane_ = new views::TabbedPane();
  tabbed_pane_->set_listener(this);
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
  AddButton("Tab 1");
  AddButton("Tab 2");

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

void TabbedPaneExample::ButtonPressed(views::Button* sender,
                                      const views::Event& event) {
  if (sender == add_) {
    AddButton("Added");
  } else if (sender == add_at_) {
    const std::wstring label = L"Added at 1";
    tabbed_pane_->AddTabAtIndex(1, WideToUTF16Hack(label),
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

void TabbedPaneExample::TabSelectedAt(int index) {
  // Just print the status when selection changes.
  PrintStatus();
}

void TabbedPaneExample::PrintStatus() {
  ExampleBase::PrintStatus("Tab Count:%d, Selected Tab:%d",
                           tabbed_pane_->GetTabCount(),
                           tabbed_pane_->GetSelectedTabIndex());
}

void TabbedPaneExample::AddButton(const std::string& label) {
  views::TextButton* button = new views::TextButton(NULL, ASCIIToWide(label));
  tabbed_pane_->AddTab(ASCIIToUTF16(label), button);
}

}  // namespace examples
