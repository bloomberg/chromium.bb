// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_EXAMPLES_EXAMPLE_BASE_H_
#define VIEWS_EXAMPLES_EXAMPLE_BASE_H_

#include <string>

#include "base/basictypes.h"

namespace views {
class View;
}  // namespace views

namespace examples {

class ExamplesMain;

class ExampleBase {
 public:
  // Returns the view containing this example controls.
  // This view is added as a tab to the example application.
  views::View* GetExampleView() {
    return container_;
  }

  // Sub-classes should creates and add the views to the given parent.
  virtual void CreateExampleView(views::View* parent) = 0;

 protected:
  explicit ExampleBase(ExamplesMain* main);
  virtual ~ExampleBase() {}

  // Sub-classes should return the name of this test.
  // It is used as the title of the tab displaying this test's controls.
  virtual std::wstring GetExampleTitle() = 0;

  // Prints a message in the status area, at the bottom of the window.
  void PrintStatus(const wchar_t* format, ...);

  // Converts an integer/boolean to wchat "on"/"off".
  static const wchar_t* IntToOnOff(int value) {
    return value ? L"on" : L"off";
  }

 private:
  // The runner actually running this test.
  ExamplesMain* main_;

  // The view containing example views.
  views::View* container_;

  DISALLOW_COPY_AND_ASSIGN(ExampleBase);
};

}  // namespace examples

#endif  // VIEWS_EXAMPLES_EXAMPLE_BASE_H_

