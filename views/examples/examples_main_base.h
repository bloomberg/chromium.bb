// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_EXAMPLES_EXAMPLE_MAIN_BASE_H_
#define VIEWS_EXAMPLES_EXAMPLE_MAIN_BASE_H_

#include "base/basictypes.h"

namespace views {
class Widget;
}  // namespace views

namespace examples {

// ExamplesMainBase creates all view examples and start event loop.
// Each platform specific main class should extend this base class and
// provide the implementation of CreateTopLevelWidget. The main program
// has to perform platform specific initializations before calling Run().
class ExamplesMainBase {
 public:
  ExamplesMainBase() {}
  virtual ~ExamplesMainBase() {}

  // Creates all examples and start UI event loop.
  void Run();

 protected:
  // Returns a widget for a top level, decorated window.
  // Each platform must implement this method and return platform specific
  // widget.
  virtual views::Widget* CreateTopLevelWidget() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(ExamplesMainBase);
};

}  // namespace examples

#endif  // VIEWS_EXAMPLES_EXAMPLE_MAIN_BASE_H_

