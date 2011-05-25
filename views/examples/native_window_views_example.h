// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_EXAMPLES_NATIVE_WINDOW_VIEWS_EXAMPLE_H_
#define VIEWS_EXAMPLES_NATIVE_WINDOW_VIEWS_EXAMPLE_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "views/examples/example_base.h"

namespace examples {

class NativeWindowViewsExample : public ExampleBase {
 public:
  explicit NativeWindowViewsExample(ExamplesMain* main);
  virtual ~NativeWindowViewsExample();

  // Overridden from ExampleBase:
  virtual std::wstring GetExampleTitle() OVERRIDE;
  virtual void CreateExampleView(views::View* container) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(NativeWindowViewsExample);
};

}  // namespace examples

#endif  // VIEWS_EXAMPLES_NATIVE_WINDOW_VIEWS_EXAMPLE_H_
