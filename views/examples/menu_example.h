// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_EXAMPLES_MENU_EXAMPLE_H_
#define VIEWS_EXAMPLES_MENU_EXAMPLE_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "views/examples/example_base.h"

namespace examples {

// MenuExample demonstrates how to use the Menu class.
class MenuExample : public ExampleBase {
 public:
  explicit MenuExample(ExamplesMain* main);
  virtual ~MenuExample();

  // Overridden from ExampleBase:
  virtual std::wstring GetExampleTitle() OVERRIDE;
  virtual void CreateExampleView(views::View* container) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(MenuExample);
};

}  // namespace examples

#endif  // VIEWS_EXAMPLES_MENU_EXAMPLE_H_
