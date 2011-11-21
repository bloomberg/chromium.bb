// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/examples_main.h"

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/i18n/icu_util.h"
#include "base/process_util.h"
#include "base/stl_util.h"
#include "base/utf_string_conversions.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"
#include "ui/views/examples/bubble_example.h"
#include "ui/views/examples/button_example.h"
#include "ui/views/examples/combobox_example.h"
#include "ui/views/examples/double_split_view_example.h"
#include "ui/views/examples/link_example.h"
#include "ui/views/examples/menu_example.h"
#include "ui/views/examples/message_box_example.h"
#include "ui/views/examples/native_theme_button_example.h"
#include "ui/views/examples/native_theme_checkbox_example.h"
#include "ui/views/examples/progress_bar_example.h"
#include "ui/views/examples/radio_button_example.h"
#include "ui/views/examples/scroll_view_example.h"
#include "ui/views/examples/single_split_view_example.h"
#include "ui/views/examples/tabbed_pane_example.h"
#include "ui/views/examples/table2_example.h"
#include "ui/views/examples/text_example.h"
#include "ui/views/examples/textfield_example.h"
#include "ui/views/examples/throbber_example.h"
#include "ui/views/examples/widget_example.h"
#include "ui/views/focus/accelerator_handler.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/test/test_views_delegate.h"
#include "ui/views/widget/widget.h"
#include "views/controls/button/text_button.h"
#include "views/controls/label.h"
#include "views/controls/tabbed_pane/tabbed_pane.h"

#if defined(OS_WIN)
// TableView is not yet ported to Linux.
#include "ui/views/examples/table_example.h"
#endif

namespace examples {

ExamplesMain::ExamplesMain()
    : tabbed_pane_(NULL),
      contents_(NULL),
      status_label_(NULL)  {
}

ExamplesMain::~ExamplesMain() {
  STLDeleteElements(&examples_);
}

void ExamplesMain::Init() {
  // Creates a window with the tabbed pane for each example,
  // and a label to print messages from each example.
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

  // Keep these in alphabetical order!
  examples_.push_back(new BubbleExample(this));
  examples_.push_back(new ButtonExample(this));
  examples_.push_back(new ComboboxExample(this));
  examples_.push_back(new DoubleSplitViewExample(this));
  examples_.push_back(new LinkExample(this));
  examples_.push_back(new MenuExample(this));
  examples_.push_back(new MessageBoxExample(this));
  examples_.push_back(new NativeThemeButtonExample(this));
  examples_.push_back(new NativeThemeCheckboxExample(this));
  examples_.push_back(new ProgressBarExample(this));
  examples_.push_back(new RadioButtonExample(this));
  examples_.push_back(new ScrollViewExample(this));
  examples_.push_back(new SingleSplitViewExample(this));
  examples_.push_back(new TabbedPaneExample(this));
#if defined(OS_WIN)
  examples_.push_back(new TableExample(this));
#endif
  examples_.push_back(new Table2Example(this));
  examples_.push_back(new TextExample(this));
  examples_.push_back(new TextfieldExample(this));
  examples_.push_back(new ThrobberExample(this));
  examples_.push_back(new WidgetExample(this));

  for (std::vector<ExampleBase*>::const_iterator i(examples_.begin());
       i != examples_.end(); ++i)
    AddExample(*i);

  window->Show();
}

void ExamplesMain::SetStatus(const std::string& status) {
  status_label_->SetText(UTF8ToUTF16(status));
}

void ExamplesMain::AddExample(ExampleBase* example) {
  tabbed_pane_->AddTab(UTF8ToUTF16(example->example_title()),
                       example->example_view());
}

bool ExamplesMain::CanResize() const {
  return true;
}

bool ExamplesMain::CanMaximize() const {
  return true;
}

string16 ExamplesMain::GetWindowTitle() const {
  return ASCIIToUTF16("Views Examples");
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

}  // namespace examples

int main(int argc, char** argv) {
#if defined(OS_WIN)
  OleInitialize(NULL);
#elif defined(OS_LINUX)
  // Initializes gtk stuff.
  g_type_init();
  gtk_init(&argc, &argv);
#endif
  CommandLine::Init(argc, argv);

  base::EnableTerminationOnHeapCorruption();

  // The exit manager is in charge of calling the dtors of singleton objects.
  base::AtExitManager exit_manager;

  ui::RegisterPathProvider();
  bool icu_result = icu_util::Initialize();
  CHECK(icu_result);
  ui::ResourceBundle::InitSharedInstance("en-US");

  MessageLoop main_message_loop(MessageLoop::TYPE_UI);

  views::TestViewsDelegate delegate;

  // We do not use this header: chrome/common/chrome_switches.h
  // because that would create a bad dependency back on Chrome.
  views::Widget::SetPureViews(
      CommandLine::ForCurrentProcess()->HasSwitch("use-pure-views"));

  examples::ExamplesMain main;
  main.Init();

  views::AcceleratorHandler accelerator_handler;
  MessageLoopForUI::current()->RunWithDispatcher(&accelerator_handler);

#if defined(OS_WIN)
  OleUninitialize();
#endif
  return 0;
}
