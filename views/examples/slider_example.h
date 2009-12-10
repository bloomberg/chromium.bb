// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_EXAMPLES_SLIDER_EXAMPLE_H_
#define VIEWS_EXAMPLES_SLIDER_EXAMPLE_H_

#include "views/controls/slider/slider.h"
#include "views/examples/example_base.h"
#include "views/fill_layout.h"

namespace examples {

// SliderExample demonstrates how to use the Slider class.
class SliderExample : public ExampleBase, views::SliderListener {
 public:
  explicit SliderExample(ExamplesMain* main) : ExampleBase(main) {
  }

  virtual ~SliderExample() {}

  virtual std::wstring GetExampleTitle() {
    return L"Slider";
  }

  virtual void CreateExampleView(views::View* container) {
    const double min = 0.0;
    const double max = 100.0;
    const double step = 1.0;
    const views::Slider::StyleFlags style =
        views::Slider::STYLE_DRAW_VALUE;
    slider_ = new views::Slider(min, max, step, style, this);

    container->SetLayoutManager(new views::FillLayout);
    container->AddChildView(slider_);
  }

 private:
  virtual void SliderValueChanged(views::Slider* sender) {
    PrintStatus(L"Value: %.1f", sender->value());
  }

  views::Slider* slider_;

  DISALLOW_COPY_AND_ASSIGN(SliderExample);
};

}  // namespace examples

#endif  // VIEWS_EXAMPLES_SLIDER_EXAMPLE_H_

