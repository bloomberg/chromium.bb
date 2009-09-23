// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/examples/examples_main_base.h"

#include "app/app_paths.h"
#include "app/resource_bundle.h"
#include "base/at_exit.h"
#include "base/process_util.h"
#include "views/controls/label.h"
#include "views/focus/accelerator_handler.h"
#include "views/grid_layout.h"
#include "views/widget/widget.h"

// Examples
#include "views/examples/button_example.h"
#include "views/examples/combobox_example.h"
#include "views/examples/message_box_example.h"
#include "views/examples/radio_button_example.h"
#include "views/examples/tabbed_pane_example.h"

namespace examples {

using views::Background;
using views::ColumnSet;
using views::GridLayout;
using views::Label;
using views::TabbedPane;
using views::View;
using views::Widget;

void ExamplesMainBase::Run() {
  base::EnableTerminationOnHeapCorruption();

  // The exit manager is in charge of calling the dtors of singleton objects.
  base::AtExitManager exit_manager;

  app::RegisterPathProvider();

  // This requires chrome to be built first right now.
  // TODO(oshima): fix build to include resource file.
  ResourceBundle::InitSharedInstance(L"en-US");
  ResourceBundle::GetSharedInstance().LoadThemeResources();

  MessageLoop main_message_loop(MessageLoop::TYPE_UI);

  Widget* widget = CreateTopLevelWidget();
  widget->Init(NULL, gfx::Rect(0, 0, 500, 300));

  // Creates a window with the tabbed pane for each examples, and
  // a label to print messages from each examples.
  View* container = new View();
  container->set_background(Background::CreateStandardPanelBackground());
  GridLayout* layout = new GridLayout(container);
  container->SetLayoutManager(layout);

  widget->SetContentsView(container);

  ColumnSet* column_set = layout->AddColumnSet(0);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);

  TabbedPane* tabbed_pane = new TabbedPane();
  Label* message = new Label();

  layout->StartRow(1, 0);
  layout->AddView(tabbed_pane);
  layout->StartRow(0 /* no expand */, 0);
  layout->AddView(message);

  ButtonExample button_example(tabbed_pane, message);
  ComboboxExample combobox_example(tabbed_pane, message);
  TabbedPaneExample tabbed_pane_example(tabbed_pane, message);
  MessageBoxExample message_box_example(tabbed_pane, message);
  RadioButtonExample radio_button_example(tabbed_pane, message);

  widget->Show();

  views::AcceleratorHandler accelerator_handler;
  MessageLoopForUI::current()->Run(&accelerator_handler);
}

}  // namespace examples
