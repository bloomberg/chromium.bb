// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_NATIVE_THEME_CHECKBOX_EXAMPLE_H_
#define UI_VIEWS_EXAMPLES_NATIVE_THEME_CHECKBOX_EXAMPLE_H_
#pragma once

#include "base/basictypes.h"
#include "ui/gfx/native_theme.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/examples/example_base.h"

namespace views {
class Checkbox;
}

namespace examples {

// NativeThemeCheckboxExample exercises a Checkbox control.
class NativeThemeCheckboxExample : public ExampleBase,
                                   public views::ButtonListener {
 public:
  explicit NativeThemeCheckboxExample(ExamplesMain* main);
  virtual ~NativeThemeCheckboxExample();

  // Overridden from ExampleBase:
  virtual void CreateExampleView(views::View* container) OVERRIDE;

 private:
  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event) OVERRIDE;

  // The only control in this test.
  views::Checkbox* button_;

  int count_;

  DISALLOW_COPY_AND_ASSIGN(NativeThemeCheckboxExample);
};

}  // namespace examples

#endif  // UI_VIEWS_EXAMPLES_NATIVE_THEME_CHECKBOX_EXAMPLE_H_
