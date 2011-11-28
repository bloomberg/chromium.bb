// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_SCROLL_VIEW_EXAMPLE_H_
#define UI_VIEWS_EXAMPLES_SCROLL_VIEW_EXAMPLE_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/views/controls/button/text_button.h"
#include "ui/views/examples/example_base.h"
#include "views/controls/scroll_view.h"

namespace examples {

class ScrollViewExample : public ExampleBase,
                          public views::ButtonListener {
 public:
  explicit ScrollViewExample(ExamplesMain* main);
  virtual ~ScrollViewExample();

  // Overridden from ExampleBase:
  virtual void CreateExampleView(views::View* container) OVERRIDE;

 private:
  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event) OVERRIDE;

  // Control buttons to change the size of scrollable and jump to
  // predefined position.
  views::TextButton* wide_;
  views::TextButton* tall_;
  views::TextButton* big_square_;
  views::TextButton* small_square_;
  views::TextButton* scroll_to_;

  class ScrollableView;
  // The content of the scroll view.
  ScrollableView* scrollable_;

  // The scroll view to test.
  views::ScrollView* scroll_view_;

  DISALLOW_COPY_AND_ASSIGN(ScrollViewExample);
};

}  // namespace examples

#endif  // UI_VIEWS_EXAMPLES_SCROLL_VIEW_EXAMPLE_H_
