// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_EXAMPLES_THROBBER_EXAMPLE_H_
#define VIEWS_EXAMPLES_THROBBER_EXAMPLE_H_
#pragma once

#include "views/examples/example_base.h"

namespace views {
class View;
}  // namespace views

namespace examples {

class ThrobberExample : public ExampleBase {
 public:
  explicit ThrobberExample(ExamplesMain* main);
  virtual ~ThrobberExample();

  // Overridden from ExampleBase:
  virtual std::wstring GetExampleTitle();
  virtual void CreateExampleView(views::View* container);

 private:
  DISALLOW_COPY_AND_ASSIGN(ThrobberExample);
};

}  // namespace examples

#endif  // VIEWS_EXAMPLES_THROBBER_EXAMPLE_H_
