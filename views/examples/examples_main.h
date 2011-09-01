// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_EXAMPLES_EXAMPLE_MAIN_H_
#define VIEWS_EXAMPLES_EXAMPLE_MAIN_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "views/widget/widget_delegate.h"

namespace views {
class Label;
class View;
}  // namespace views

namespace examples {

// ExamplesMainBase creates all view examples and start event loop.
class ExamplesMain : public views::WidgetDelegate {
 public:
  ExamplesMain();
  virtual ~ExamplesMain();

  // views::WidgetDelegate implementation:
  virtual bool CanResize() const OVERRIDE;
  virtual views::View* GetContentsView() OVERRIDE;
  virtual void WindowClosing() OVERRIDE;
  virtual views::Widget* GetWidget() OVERRIDE;
  virtual const views::Widget* GetWidget() const OVERRIDE;

  // Prints a message in the status area, at the bottom of the window.
  void SetStatus(const std::string& status);

  // Creates all examples and runs the UI event loop.
  void Run();

 private:
  views::View* contents_;

  views::Label* status_label_;

  DISALLOW_COPY_AND_ASSIGN(ExamplesMain);
};

}  // namespace examples

#endif  // VIEWS_EXAMPLES_EXAMPLE_MAIN_H_

