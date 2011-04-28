// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/examples/button_example.h"

#include "views/layout/fill_layout.h"
#include "views/view.h"

namespace examples {

ButtonExample::ButtonExample(ExamplesMain* main)
    : ExampleBase(main),
      count_(0) {
}

ButtonExample::~ButtonExample() {
}

std::wstring ButtonExample::GetExampleTitle() {
  return L"Text Button";
}

void ButtonExample::CreateExampleView(views::View* container) {
  button_ = new views::TextButton(this, L"Button");
  container->SetLayoutManager(new views::FillLayout);
  container->AddChildView(button_);
}

void ButtonExample::ButtonPressed(views::Button* sender,
                                  const views::Event& event) {
  PrintStatus(L"Pressed! count:%d", ++count_);
}

}  // namespace examples
