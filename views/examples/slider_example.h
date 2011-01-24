// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_EXAMPLES_SLIDER_EXAMPLE_H_
#define VIEWS_EXAMPLES_SLIDER_EXAMPLE_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "views/controls/slider/slider.h"
#include "views/examples/example_base.h"

namespace examples {

// SliderExample demonstrates how to use the Slider class.
class SliderExample : public ExampleBase, public views::SliderListener {
 public:
  explicit SliderExample(ExamplesMain* main);
  virtual ~SliderExample();

  // Overridden from ExampleBase:
  virtual std::wstring GetExampleTitle() OVERRIDE;
  virtual void CreateExampleView(views::View* container) OVERRIDE;

 private:
  // Overridden from views::SliderListener:
  virtual void SliderValueChanged(views::Slider* sender) OVERRIDE;

  views::Slider* slider_;

  DISALLOW_COPY_AND_ASSIGN(SliderExample);
};

}  // namespace examples

#endif  // VIEWS_EXAMPLES_SLIDER_EXAMPLE_H_
