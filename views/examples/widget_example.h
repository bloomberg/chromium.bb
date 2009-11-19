// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_EXAMPLES_WIDGET_EXAMPLE_H_
#define VIEWS_EXAMPLES_WIDGET_EXAMPLE_H_

#include "views/background.h"
#include "views/controls/button/text_button.h"
#include "views/examples/example_base.h"
#include "views/fill_layout.h"
#include "views/view.h"
#include "views/widget/root_view.h"
#include "views/widget/widget.h"

namespace examples {

// WidgetExample demonstrates how to create a popup widget.
class WidgetExample : public ExampleBase, public views::ButtonListener {
 public:
  explicit WidgetExample(ExamplesMain* main) : ExampleBase(main) {
  }

  virtual ~WidgetExample() {}

  virtual std::wstring GetExampleTitle() {
    return L"Widget";
  }

  virtual void CreateExampleView(views::View* container) {
    create_button_ = new views::TextButton(this, L"Create a popup widget");
    container->SetLayoutManager(new views::FillLayout);
    container->AddChildView(create_button_);
    // We'll create close_button_ and popup_widget_, when the create button
    // is clicked.
    close_button_ = NULL;
    popup_widget_ = NULL;
  }

 private:
  // ButtonListner implementation.
  virtual void ButtonPressed(views::Button* sender, const views::Event& event) {
    if (sender == create_button_) {
      // Disable the create button.
      create_button_->SetEnabled(false);

      // Create a popup widget.
      popup_widget_ = views::Widget::CreatePopupWidget(
        views::Widget::NotTransparent,
        views::Widget::AcceptEvents,
        views::Widget::DeleteOnDestroy);
      popup_widget_->Init(NULL, gfx::Rect(100, 100, 200, 300));

      // Add a button to close the popup widget.
      close_button_ = new views::TextButton(this, L"Close");
      views::View* widget_container = new views::View;
      widget_container->SetLayoutManager(new views::FillLayout);
      widget_container->set_background(
        views::Background::CreateStandardPanelBackground());
      widget_container->AddChildView(close_button_);
      popup_widget_->GetRootView()->SetContentsView(widget_container);

      // Show the popup widget.
      popup_widget_->Show();
    } else if (sender == close_button_) {
      // Close the popup widget. This will delete popup_widget_ as
      // DeleteOnDestroy is specified when the widget was created.
      // Views on the widget will also be deleted.
      popup_widget_->Close();
      // Re-enable the create button.
      create_button_->SetEnabled(true);
      close_button_ = NULL;
      popup_widget_ = NULL;
    }
  }

  views::TextButton* create_button_;
  views::TextButton* close_button_;
  views::Widget* popup_widget_;

  DISALLOW_COPY_AND_ASSIGN(WidgetExample);
};

}  // namespace examples

#endif  // VIEWS_EXAMPLES_WIDGET_EXAMPLE_H_

