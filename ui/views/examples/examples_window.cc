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
#include "ui/views/controls/button/text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/tabbed_pane/tabbed_pane.h"
#include "ui/views/examples/bubble_example.h"
#include "ui/views/examples/button_example.h"
#include "ui/views/examples/combobox_example.h"
#include "ui/views/examples/double_split_view_example.h"
#include "ui/views/examples/link_example.h"
#include "ui/views/examples/message_box_example.h"
#include "ui/views/examples/native_theme_button_example.h"
#include "ui/views/examples/native_theme_checkbox_example.h"
#include "ui/views/examples/progress_bar_example.h"
#include "ui/views/examples/radio_button_example.h"
#include "ui/views/examples/scroll_view_example.h"
#include "ui/views/examples/single_split_view_example.h"
#include "ui/views/examples/tabbed_pane_example.h"
#include "ui/views/examples/text_example.h"
#include "ui/views/examples/textfield_example.h"
#include "ui/views/examples/throbber_example.h"
#include "ui/views/examples/widget_example.h"
#include "ui/views/focus/accelerator_handler.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/test/test_views_delegate.h"
#include "ui/views/widget/widget.h"

#if !defined(USE_AURA)
#include "ui/views/examples/menu_example.h"
#include "ui/views/examples/table2_example.h"
#if defined(OS_WIN)
#include "ui/views/examples/table_example.h"
#endif
#endif


namespace views {
namespace examples {

class ExampleBase;

class ExamplesWindowContents : public views::WidgetDelegateView {
 public:
  explicit ExamplesWindowContents(bool quit_on_close)
      : tabbed_pane_(new views::TabbedPane),
        status_label_(new views::Label),
        quit_on_close_(quit_on_close) {
    instance_ = this;
  }
  virtual ~ExamplesWindowContents() {
  }

  // Prints a message in the status area, at the bottom of the window.
  void SetStatus(const std::string& status) {
    status_label_->SetText(UTF8ToUTF16(status));
  }

  static ExamplesWindowContents* instance() { return instance_; }

 private:
  // Overridden from WidgetDelegateView:
  virtual bool CanResize() const OVERRIDE { return true; }
  virtual bool CanMaximize() const OVERRIDE { return true; }
  virtual string16 GetWindowTitle() const OVERRIDE {
    return ASCIIToUTF16("Views Examples");
  }
  virtual views::View* GetContentsView() OVERRIDE { return this; }
  virtual void WindowClosing() OVERRIDE {
    instance_ = NULL;
    if (quit_on_close_)
      MessageLoopForUI::current()->Quit();
  }

  // Overridden from View:
  virtual void ViewHierarchyChanged(bool is_add, View* parent, View* child) {
    if (is_add && child == this)
      InitExamplesWindow();
  }

  // Creates the layout within the examples window.
  void InitExamplesWindow() {
    set_background(views::Background::CreateStandardPanelBackground());
    views::GridLayout* layout = new views::GridLayout(this);
    SetLayoutManager(layout);
    views::ColumnSet* column_set = layout->AddColumnSet(0);
    column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1,
                          views::GridLayout::USE_PREF, 0, 0);
    layout->StartRow(1, 0);
    layout->AddView(tabbed_pane_);
    layout->StartRow(0 /* no expand */, 0);
    layout->AddView(status_label_);

    AddExamples();
  }

  // Adds all the individual examples to the tab strip.
  void AddExamples() {
    AddExample(new BubbleExample);
    AddExample(new ButtonExample);
    AddExample(new ComboboxExample);
    AddExample(new DoubleSplitViewExample);
    AddExample(new LinkExample);
#if !defined(USE_AURA)
    AddExample(new MenuExample);
#endif
    AddExample(new MessageBoxExample);
    AddExample(new NativeThemeButtonExample);
    AddExample(new NativeThemeCheckboxExample);
    AddExample(new ProgressBarExample);
    AddExample(new RadioButtonExample);
    AddExample(new ScrollViewExample);
    AddExample(new SingleSplitViewExample);
    AddExample(new TabbedPaneExample);
#if !defined(USE_AURA)
#if defined(OS_WIN)
    AddExample(new TableExample);
#endif
    AddExample(new Table2Example);
#endif
    AddExample(new TextExample);
    AddExample(new TextfieldExample);
    AddExample(new ThrobberExample);
    AddExample(new WidgetExample);
  }

  // Adds a new example to the tabbed window.
  void AddExample(ExampleBase* example) {
    tabbed_pane_->AddTab(UTF8ToUTF16(example->example_title()),
                         example->example_view());
  }

  static ExamplesWindowContents* instance_;
  views::TabbedPane* tabbed_pane_;
  views::Label* status_label_;
  bool quit_on_close_;

  DISALLOW_COPY_AND_ASSIGN(ExamplesWindowContents);
};

// static
ExamplesWindowContents* ExamplesWindowContents::instance_ = NULL;

void ShowExamplesWindow(bool quit_on_close) {
  if (ExamplesWindowContents::instance()) {
    ExamplesWindowContents::instance()->GetWidget()->Activate();
  } else {
    Widget::CreateWindowWithBounds(new ExamplesWindowContents(quit_on_close),
                                   gfx::Rect(0, 0, 850, 300))->Show();
  }
}

void LogStatus(const std::string& string) {
  ExamplesWindowContents::instance()->SetStatus(string);
}

}  // namespace examples
}  // namespace views
