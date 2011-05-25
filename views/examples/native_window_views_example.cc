// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/examples/native_window_views_example.h"

#include "ui/gfx/canvas.h"
#include "views/examples/example_base.h"
#include "views/controls/button/text_button.h"
#include "views/controls/label.h"
#include "views/layout/grid_layout.h"
#include "views/view.h"
#include "views/window/native_window_views.h"
#include "views/window/window.h"
#include "views/window/window_delegate.h"

namespace examples {

class WindowContentView : public views::View,
                          public views::WindowDelegate,
                          public views::ButtonListener {
 public:
   WindowContentView()
        : ALLOW_THIS_IN_INITIALIZER_LIST(
              button_(new views::TextButton(this, L"Click me!"))),
              label_(new views::Label(L"Some label")) {
    views::GridLayout* layout = new views::GridLayout(this);
    views::ColumnSet* columns = layout->AddColumnSet(0);
    columns->AddColumn(views::GridLayout::FILL,
                       views::GridLayout::FILL,
                       1,
                       views::GridLayout::USE_PREF,
                       0,
                       0);
    SetLayoutManager(layout);
    layout->StartRow(0, 0);
    layout->AddView(button_);
    layout->StartRow(1, 0);
    layout->AddView(label_);
  }
  virtual ~WindowContentView() {}

  // Overridden from views::View:
  virtual void OnPaint(gfx::Canvas* canvas) {
    canvas->FillRectInt(SK_ColorWHITE, 0, 0, width(), height());
  }

  // Overridden from views::WindowDelegate:
  virtual std::wstring GetWindowTitle() const {
    return L"Example NativeWindowViews";
  }
  virtual View* GetContentsView() {
    return this;
  }

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender, const views::Event& event) {
    if (sender == button_)
      label_->SetText(L"Button Clicked!");
  }

 private:
  views::TextButton* button_;
  views::Label* label_;

  DISALLOW_COPY_AND_ASSIGN(WindowContentView);
};

NativeWindowViewsExample::NativeWindowViewsExample(ExamplesMain* main)
    : ExampleBase(main) {
}

NativeWindowViewsExample::~NativeWindowViewsExample() {
}

std::wstring NativeWindowViewsExample::GetExampleTitle() {
  return L"NativeWindowViews";
}

void NativeWindowViewsExample::CreateExampleView(views::View* container) {
  views::Window* window = new views::Window;
  views::NativeWindowViews* nwv =
      new views::NativeWindowViews(container, window);
  views::Window::InitParams params(new WindowContentView);
  params.native_window = nwv;
  params.widget_init_params.bounds = gfx::Rect(20, 20, 600, 300);
  window->InitWindow(params);
  window->Show();
}

}  // namespace examples
