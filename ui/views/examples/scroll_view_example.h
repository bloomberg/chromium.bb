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

namespace views {
namespace examples {

class ScrollViewExample : public ExampleBase, public ButtonListener {
 public:
  ScrollViewExample();
  virtual ~ScrollViewExample();

  // Overridden from ExampleBase:
  virtual void CreateExampleView(View* container) OVERRIDE;

 private:
  // Overridden from ButtonListener:
  virtual void ButtonPressed(Button* sender, const Event& event) OVERRIDE;

  // Control buttons to change the size of scrollable and jump to
  // predefined position.
  TextButton* wide_;
  TextButton* tall_;
  TextButton* big_square_;
  TextButton* small_square_;
  TextButton* scroll_to_;

  class ScrollableView;
  // The content of the scroll view.
  ScrollableView* scrollable_;

  // The scroll view to test.
  ScrollView* scroll_view_;

  DISALLOW_COPY_AND_ASSIGN(ScrollViewExample);
};

}  // namespace examples
}  // namespace views

#endif  // UI_VIEWS_EXAMPLES_SCROLL_VIEW_EXAMPLE_H_
