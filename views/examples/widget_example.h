// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_EXAMPLES_WIDGET_EXAMPLE_H_
#define VIEWS_EXAMPLES_WIDGET_EXAMPLE_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "views/controls/button/text_button.h"
#include "views/examples/example_base.h"
#include "views/widget/widget.h"

namespace examples {

// WidgetExample demonstrates how to create a popup widget.
class WidgetExample : public ExampleBase, public views::ButtonListener {
 public:
  explicit WidgetExample(ExamplesMain* main);
  virtual ~WidgetExample();

  // Overridden from ExampleBase:
  virtual std::wstring GetExampleTitle() OVERRIDE;
  virtual void CreateExampleView(views::View* container) OVERRIDE;

 private:
  enum Command {
    POPUP,
    CHILD,
    TRANSPARENT_POPUP,
    TRANSPARENT_CHILD,
    CLOSE_WIDGET,
  };

  void BuildButton(views::View* container, const std::wstring& label, int tag);

  void InitWidget(views::Widget* widget,
                  const views::Widget::TransparencyParam transparency);

#if defined(OS_LINUX)
  void CreateChild(views::View* parent,
                   const views::Widget::TransparencyParam transparency);
#endif

  void CreatePopup(views::View* parent,
                   const views::Widget::TransparencyParam transparency);

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(WidgetExample);
};

}  // namespace examples

#endif  // VIEWS_EXAMPLES_WIDGET_EXAMPLE_H_
