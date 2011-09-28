// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_EXAMPLES_EXAMPLE_BASE_H_
#define VIEWS_EXAMPLES_EXAMPLE_BASE_H_
#pragma once

#include <string>

#include "base/basictypes.h"

namespace views {
class View;
}

namespace examples {

class ExamplesMain;

class ExampleBase {
 public:
  virtual ~ExampleBase();

  // Returns the view containing this example controls.
  // This view is added as a tab to the example application.
  views::View* GetExampleView() { return container_; }

  // Sub-classes should creates and add the views to the given parent.
  virtual void CreateExampleView(views::View* parent) = 0;

  const std::string& example_title() const { return example_title_; }

 protected:
  ExampleBase(ExamplesMain* main, const char* title);

  // Prints a message in the status area, at the bottom of the window.
  void PrintStatus(const char* format, ...);

  // Converts an boolean value to "on" or "off".
  const char* BoolToOnOff(bool value) {
    return value ? "on" : "off";
  }

 private:
  // The runner actually running this test.
  ExamplesMain* main_;

  // Name of the example - used for the title of the tab.
  std::string example_title_;

  // The view containing example views.
  views::View* container_;

  DISALLOW_COPY_AND_ASSIGN(ExampleBase);
};

}  // namespace examples

#endif  // VIEWS_EXAMPLES_EXAMPLE_BASE_H_
