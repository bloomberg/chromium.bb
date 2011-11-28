// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_PROGRESS_BAR_EXAMPLE_H_
#define UI_VIEWS_EXAMPLES_PROGRESS_BAR_EXAMPLE_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/examples/example_base.h"

namespace views {
class ProgressBar;
}

namespace examples {

class ProgressBarExample : public ExampleBase,
                           public views::ButtonListener {
 public:
  explicit ProgressBarExample(ExamplesMain* main);
  virtual ~ProgressBarExample();

  // Overridden from ExampleBase:
  virtual void CreateExampleView(views::View* container) OVERRIDE;

 private:
  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* button,
                             const views::Event& event) OVERRIDE;

  views::Button* minus_button_;
  views::Button* plus_button_;
  views::ProgressBar* progress_bar_;
  double current_percent_;

  DISALLOW_COPY_AND_ASSIGN(ProgressBarExample);
};

}  // namespace examples

#endif  // UI_VIEWS_EXAMPLES_PROGRESS_BAR_EXAMPLE_H_
