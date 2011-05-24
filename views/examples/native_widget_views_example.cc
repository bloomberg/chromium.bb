// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/examples/native_widget_views_example.h"

#include "ui/gfx/canvas.h"
#include "views/controls/button/text_button.h"
#include "views/examples/example_base.h"
#include "views/view.h"
#include "views/widget/widget.h"
#include "views/widget/native_widget_views.h"

namespace examples {

// A ContentView for our example widget. Contains a variety of controls for
// testing NativeWidgetViews event handling. If any part of the Widget's bounds
// are rendered red, something went wrong.
class TestContentView : public views::View,
                        public views::ButtonListener {
 public:
  TestContentView()
      : click_count_(0),
        ALLOW_THIS_IN_INITIALIZER_LIST(
            button_(new views::TextButton(this, L"Click me!"))) {
    AddChildView(button_);
  }
  virtual ~TestContentView() {
  }

  // Overridden from views::View:
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {
    SkColor color = click_count_ % 2 == 0 ? SK_ColorGREEN : SK_ColorBLUE;
    canvas->FillRectInt(color, 0, 0, width(), height());
  }
  virtual void Layout() OVERRIDE {
    button_->SetBounds(10, 10, width() - 20, height() - 20);
  }

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event) OVERRIDE {
    if (sender == button_) {
      ++click_count_;
      SchedulePaint();
    }
  }

 private:
  int click_count_;
  views::TextButton* button_;

  DISALLOW_COPY_AND_ASSIGN(TestContentView);
};

NativeWidgetViewsExample::NativeWidgetViewsExample(ExamplesMain* main)
    : ExampleBase(main) {
}

NativeWidgetViewsExample::~NativeWidgetViewsExample() {
}

std::wstring NativeWidgetViewsExample::GetExampleTitle() {
  return L"NativeWidgetViews";
}

void NativeWidgetViewsExample::CreateExampleView(views::View* container) {
  views::Widget* widget = new views::Widget;
  views::NativeWidgetViews* nwv =
      new views::NativeWidgetViews(container, widget);
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_CONTROL);
  params.native_widget = nwv;
  widget->Init(params);
  widget->SetContentsView(new TestContentView);
  widget->SetBounds(gfx::Rect(10, 10, 300, 150));
}

}  // namespace examples
