// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_EXAMPLES_EXAMPLE_MAIN_H_
#define VIEWS_EXAMPLES_EXAMPLE_MAIN_H_

#include <string>

#include "base/basictypes.h"
#include "views/window/window_delegate.h"

namespace views {
class Label;
class View;
}  // namespace views

namespace examples {

// ExamplesMainBase creates all view examples and start event loop.
class ExamplesMain : public views::WindowDelegate {
 public:
  ExamplesMain() : contents_(NULL), status_label_(NULL) {}
  virtual ~ExamplesMain() {}

  // views::WindowDelegate implementation:
  virtual views::View* GetContentsView();

  // Prints a message in the status area, at the bottom of the window.
  void SetStatus(const std::wstring& status);

  // Creates all examples and runs the UI event loop.
  void Run();

 private:
  views::View* contents_;

  views::Label* status_label_;

  DISALLOW_COPY_AND_ASSIGN(ExamplesMain);
};

}  // namespace examples

#endif  // VIEWS_EXAMPLES_EXAMPLE_MAIN_H_

