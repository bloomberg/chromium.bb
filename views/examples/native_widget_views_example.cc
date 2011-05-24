// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/examples/native_widget_views_example.h"

#include "views/examples/example_base.h"
#include "views/view.h"
#include "views/widget/widget.h"
#include "views/widget/native_widget_views.h"

namespace examples {

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
  views::NativeWidgetViews* nwv = new views::NativeWidgetViews(widget);
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_CONTROL);
  params.native_widget = nwv;
  widget->Init(params);
  container->AddChildView(nwv->GetView());
  widget->SetBounds(gfx::Rect(10, 10, 50, 50));
}

}  // namespace examples
