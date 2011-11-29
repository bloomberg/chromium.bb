// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_LINK_EXAMPLE_H_
#define UI_VIEWS_EXAMPLES_LINK_EXAMPLE_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/views/controls/link_listener.h"
#include "ui/views/examples/example_base.h"

namespace views {
class View;
}

namespace examples {

class LinkExample : public ExampleBase,
                    public views::LinkListener {
 public:
  explicit LinkExample(ExamplesMain* main);
  virtual ~LinkExample();

  // Overridden from ExampleBase:
  virtual void CreateExampleView(views::View* container) OVERRIDE;

 private:
  // Overridden from views::LinkListener:
  virtual void LinkClicked(views::Link* source, int event_flags) OVERRIDE;

  views::Link* link_;

  DISALLOW_COPY_AND_ASSIGN(LinkExample);
};

}  // namespace examples

#endif  // UI_VIEWS_EXAMPLES_LINK_EXAMPLE_H_
