// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/slider_example.h"

#include <memory>

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/slider.h"
#include "ui/views/examples/grit/views_examples_resources.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"

namespace views {
namespace examples {

SliderExample::SliderExample()
    : ExampleBase(l10n_util::GetStringUTF8(IDS_SLIDER_SELECT_LABEL).c_str()) {}

SliderExample::~SliderExample() = default;

void SliderExample::CreateExampleView(View* container) {
  label_ = container->AddChildView(std::make_unique<Label>());
  slider_ = container->AddChildView(std::make_unique<Slider>(this));

  slider_->SetValue(0.5);

  container->SetLayoutManager(std::make_unique<BoxLayout>(
      BoxLayout::Orientation::kHorizontal, gfx::Insets(3), 3));
}

void SliderExample::SliderValueChanged(Slider* sender,
                                       float value,
                                       float old_value,
                                       SliderChangeReason reason) {
  label_->SetText(base::ASCIIToUTF16(base::StringPrintf("%.3lf", value)));
}

}  // namespace examples
}  // namespace views
