// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/link_example.h"

#include "base/utf_string_conversions.h"
#include "ui/views/controls/link.h"
#include "ui/views/layout/fill_layout.h"
#include "views/view.h"

namespace examples {

LinkExample::LinkExample(ExamplesMain* main)
    : ExampleBase(main, "Link") {
}

LinkExample::~LinkExample() {
}

void LinkExample::CreateExampleView(views::View* container) {
  link_ = new views::Link(ASCIIToUTF16("Click me!"));
  link_->set_listener(this);

  container->SetLayoutManager(new views::FillLayout);
  container->AddChildView(link_);
}

void LinkExample::LinkClicked(views::Link* source, int event_flags) {
  PrintStatus("Link clicked");
}

}  // namespace examples
