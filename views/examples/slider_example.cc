// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/examples/slider_example.h"

#include "build/build_config.h"
#include "views/layout/fill_layout.h"

namespace examples {

SliderExample::SliderExample(ExamplesMain* main) : ExampleBase(main) {
}

SliderExample::~SliderExample() {
}

std::wstring SliderExample::GetExampleTitle() {
  return L"Slider";
}

void SliderExample::CreateExampleView(views::View* container) {
#if !defined(OS_WIN) && !defined(OS_MACOSX)
  const double min = 0.0;
  const double max = 100.0;
  const double step = 1.0;
  slider_ = new views::Slider(min, max, step,
                              views::Slider::STYLE_DRAW_VALUE, this);

  container->SetLayoutManager(new views::FillLayout);
  container->AddChildView(slider_);
#endif
}

void SliderExample::SliderValueChanged(views::Slider* sender) {
  PrintStatus(L"Value: %.1f", sender->value());
}

}  // namespace examples
