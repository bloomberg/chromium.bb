// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_WIDGET_EXAMPLE_H_
#define UI_VIEWS_EXAMPLES_WIDGET_EXAMPLE_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/views/controls/button/text_button.h"
#include "ui/views/examples/example_base.h"
#include "ui/views/widget/widget.h"

namespace views {
namespace examples {

// WidgetExample demonstrates how to create a popup widget.
class WidgetExample : public ExampleBase, public ButtonListener {
 public:
  WidgetExample();
  virtual ~WidgetExample();

  // Overridden from ExampleBase:
  virtual void CreateExampleView(View* container) OVERRIDE;

 private:
  enum Command {
    POPUP,
    CHILD,
    TRANSPARENT_POPUP,
    TRANSPARENT_CHILD,
    CLOSE_WIDGET,
  };

  void BuildButton(View* container, const std::string& label, int tag);

  void InitWidget(Widget* widget, bool transparent);

#if defined(OS_LINUX)
  void CreateChild(View* parent, bool transparent);
#endif

  void CreatePopup(View* parent, bool transparent);

  // Overridden from ButtonListener:
  virtual void ButtonPressed(Button* sender, const Event& event) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(WidgetExample);
};

}  // namespace examples
}  // namespace views

#endif  // UI_VIEWS_EXAMPLES_WIDGET_EXAMPLE_H_
