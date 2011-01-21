// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_EXAMPLES_TABBED_PANE_EXAMPLE_H_
#define VIEWS_EXAMPLES_TABBED_PANE_EXAMPLE_H_
#pragma once

#include "views/controls/button/text_button.h"
#include "views/examples/example_base.h"
#include "views/controls/tabbed_pane/tabbed_pane.h"

namespace examples {

// A TabbedPane example tests adding/removing/selecting tabs.
class TabbedPaneExample : public ExampleBase,
                          public views::ButtonListener,
                          public views::TabbedPane::Listener {
 public:
  explicit TabbedPaneExample(ExamplesMain* main);
  virtual ~TabbedPaneExample();

  // Overridden from ExampleBase:
  virtual std::wstring GetExampleTitle();
  virtual void CreateExampleView(views::View* container);

 private:
  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);

  // Overridden from views::TabbedPane::Listener:
  virtual void TabSelectedAt(int index);

  // Print the status of the tab in the status area.
  void PrintStatus();

  void AddButton(const std::wstring& label);

  // The tabbed pane to be tested.
  views::TabbedPane* tabbed_pane_;

  // Control buttons to add, remove or select tabs.
  views::Button* add_, *add_at_, *remove_at_, *select_at_;

  DISALLOW_COPY_AND_ASSIGN(TabbedPaneExample);
};

}  // namespace examples

#endif  // VIEWS_EXAMPLES_TABBED_PANE_EXAMPLE_H_
