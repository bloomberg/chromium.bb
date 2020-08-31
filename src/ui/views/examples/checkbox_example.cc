// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/checkbox_example.h"

#include <memory>

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/radio_button.h"
#include "ui/views/examples/examples_window.h"
#include "ui/views/layout/fill_layout.h"

namespace views {
namespace examples {

CheckboxExample::CheckboxExample() : ExampleBase("Checkbox") {}

CheckboxExample::~CheckboxExample() = default;

void CheckboxExample::CreateExampleView(View* container) {
  container->SetLayoutManager(std::make_unique<FillLayout>());
  button_ = container->AddChildView(
      std::make_unique<Checkbox>(base::ASCIIToUTF16("Checkbox"), this));
}

void CheckboxExample::ButtonPressed(Button* sender, const ui::Event& event) {
  PrintStatus("Pressed! count: %d", ++count_);
}

}  // namespace examples
}  // namespace views
