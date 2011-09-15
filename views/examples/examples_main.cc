// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/examples/examples_main.h"

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/i18n/icu_util.h"
#include "base/process_util.h"
#include "base/utf_string_conversions.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"
#include "views/controls/button/text_button.h"
#include "views/controls/label.h"
#include "views/controls/tabbed_pane/tabbed_pane.h"
#include "views/examples/button_example.h"
#include "views/examples/combobox_example.h"
#include "views/examples/double_split_view_example.h"
#include "views/examples/link_example.h"
#include "views/examples/menu_example.h"
#include "views/examples/message_box_example.h"
#include "views/examples/native_theme_button_example.h"
#include "views/examples/native_theme_checkbox_example.h"
#include "views/examples/native_widget_views_example.h"
#include "views/examples/radio_button_example.h"
#include "views/examples/scroll_view_example.h"
#include "views/examples/single_split_view_example.h"
#include "views/examples/tabbed_pane_example.h"
#include "views/examples/table2_example.h"
#include "views/examples/textfield_example.h"
#include "views/examples/throbber_example.h"
#include "views/examples/widget_example.h"
#include "views/focus/accelerator_handler.h"
#include "views/layout/grid_layout.h"
#include "views/test/test_views_delegate.h"
#include "views/widget/widget.h"

#if defined(OS_WIN)
// TableView is not yet ported to Linux.
#include "views/examples/table_example.h"
#endif

namespace examples {

ExamplesMain::ExamplesMain()
    : tabbed_pane_(NULL),
      contents_(NULL),
      status_label_(NULL) {}

ExamplesMain::~ExamplesMain() {}

bool ExamplesMain::CanResize() const {
  return true;
}

views::View* ExamplesMain::GetContentsView() {
  return contents_;
}

void ExamplesMain::WindowClosing() {
  MessageLoopForUI::current()->Quit();
}

views::Widget* ExamplesMain::GetWidget() {
  return contents_->GetWidget();
}

const views::Widget* ExamplesMain::GetWidget() const {
  return contents_->GetWidget();
}

void ExamplesMain::SetStatus(const std::string& status) {
  status_label_->SetText(UTF8ToWide(status));
}

void ExamplesMain::Run() {
  base::EnableTerminationOnHeapCorruption();

  // The exit manager is in charge of calling the dtors of singleton objects.
  base::AtExitManager exit_manager;

  ui::RegisterPathProvider();
  icu_util::Initialize();

  ResourceBundle::InitSharedInstance("en-US");

  MessageLoop main_message_loop(MessageLoop::TYPE_UI);

  // Creates a window with the tabbed pane for each examples, and
  // a label to print messages from each examples.
  DCHECK(contents_ == NULL) << "Run called more than once.";
  contents_ = new views::View();
  contents_->set_background(views::Background::CreateStandardPanelBackground());
  views::GridLayout* layout = new views::GridLayout(contents_);
  contents_->SetLayoutManager(layout);

  views::ColumnSet* column_set = layout->AddColumnSet(0);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1,
                        views::GridLayout::USE_PREF, 0, 0);

  tabbed_pane_ = new views::TabbedPane();
  status_label_ = new views::Label();

  layout->StartRow(1, 0);
  layout->AddView(tabbed_pane_);
  layout->StartRow(0 /* no expand */, 0);
  layout->AddView(status_label_);

  // TODO(satorux): The window is getting wide.  Eventually, we would have
  // the second tabbed pane.
  views::Widget* window =
      views::Widget::CreateWindowWithBounds(this, gfx::Rect(0, 0, 850, 300));

  NativeThemeCheckboxExample native_theme_checkbox_example(this);
  AddExample(&native_theme_checkbox_example);

  NativeThemeButtonExample native_theme_button_example(this);
  AddExample(&native_theme_button_example);

  NativeWidgetViewsExample native_widget_views_example(this);
  AddExample(&native_widget_views_example);

  TextfieldExample textfield_example(this);
  AddExample(&textfield_example);

  ButtonExample button_example(this);
  AddExample(&button_example);

  ThrobberExample throbber_example(this);
  AddExample(&throbber_example);

  ComboboxExample combobox_example(this);
  AddExample(&combobox_example);

  LinkExample link_example(this);
  AddExample(&link_example);

  TabbedPaneExample tabbed_pane_example(this);
  AddExample(&tabbed_pane_example);

  MessageBoxExample message_box_example(this);
  AddExample(&message_box_example);

  RadioButtonExample radio_button_example(this);
  AddExample(&radio_button_example);

  ScrollViewExample scroll_view_example(this);
  AddExample(&scroll_view_example);

  SingleSplitViewExample single_split_view_example(this);
  AddExample(&single_split_view_example);

  DoubleSplitViewExample double_split_view_example(this);
  AddExample(&double_split_view_example);

#if defined(OS_WIN)
  TableExample table_example(this);
  AddExample(&table_example);
#endif

  Table2Example table2_example(this);
  AddExample(&table2_example);

  WidgetExample widget_example(this);
  AddExample(&widget_example);

  MenuExample menu_example(this);
  AddExample(&widget_example);

  window->Show();
  views::AcceleratorHandler accelerator_handler;
  MessageLoopForUI::current()->Run(&accelerator_handler);
}

void ExamplesMain::AddExample(ExampleBase* example) {
  tabbed_pane_->AddTab(example->GetExampleTitle(), example->GetExampleView());
}

}  // namespace examples

int main(int argc, char** argv) {
#if defined(OS_WIN)
  OleInitialize(NULL);
#elif defined(OS_LINUX)
  // Initializes gtk stuff.
  g_thread_init(NULL);
  g_type_init();
  gtk_init(&argc, &argv);
#endif
  views::TestViewsDelegate delegate;

  CommandLine::Init(argc, argv);

  // We do not use this header: chrome/common/chrome_switches.h
  // because that would create a bad dependency back on Chrome.
  views::Widget::SetPureViews(
      CommandLine::ForCurrentProcess()->HasSwitch("use-pure-views"));

  examples::ExamplesMain main;
  main.Run();

#if defined(OS_WIN)
  OleUninitialize();
#endif
  return 0;
}
