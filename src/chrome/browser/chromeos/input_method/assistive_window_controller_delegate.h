// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_ASSISTIVE_WINDOW_CONTROLLER_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_ASSISTIVE_WINDOW_CONTROLLER_DELEGATE_H_

namespace ui {
namespace ime {

struct AssistiveWindowButton;

}  // namespace ime
}  // namespace ui

namespace chromeos {
namespace input_method {

class AssistiveWindowControllerDelegate {
 public:
  virtual void AssistiveWindowButtonClicked(
      const ui::ime::AssistiveWindowButton& button) const = 0;

 protected:
  AssistiveWindowControllerDelegate() = default;
  virtual ~AssistiveWindowControllerDelegate() = default;
};

}  // namespace input_method
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_ASSISTIVE_WINDOW_CONTROLLER_DELEGATE_H_
