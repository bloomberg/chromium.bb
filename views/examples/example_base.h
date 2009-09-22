// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_EXAMPLES_EXAMPLE_BASE_H_
#define VIEWS_EXAMPLES_EXAMPLE_BASE_H_

#include <string>

#include "base/basictypes.h"

namespace views {
class Label;
class TabbedPane;
}  // namespace views

namespace examples {

// ExampleBase defines utility functions for examples.
class ExampleBase {
 protected:
  explicit ExampleBase(views::Label* status) : status_(status) {}

  virtual ~ExampleBase() {}

  // Prints a message in the status area, at the bottom of the window.
  void PrintStatus(const wchar_t* format, ...);

  // Converts an integer/boolean to wchat "on"/"off".
  static const wchar_t* IntToOnOff(int value) {
    return value ? L"on" : L"off";
  }

  // Adds a new tab with the button with given label to the tabbed pane.
  static void AddButton(views::TabbedPane* tabbed_pane,
                        const std::wstring& label);

 private:
  // A label to show a message at the bottom of example app.
  views::Label* status_;

  DISALLOW_COPY_AND_ASSIGN(ExampleBase);
};

}  // namespace examples

#endif  // VIEWS_EXAMPLES_EXAMPLE_BASE_H_

