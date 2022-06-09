// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_L10N_UTIL_TEST_UTIL_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_L10N_UTIL_TEST_UTIL_H_

#include <memory>
#include <string>

#include "chrome/browser/ash/input_method/mock_input_method_manager_impl.h"
#include "ui/base/ime/ash/input_method_descriptor.h"

namespace chromeos {

class MockInputMethodManagerWithInputMethods
    : public input_method::MockInputMethodManagerImpl {
 public:
  MockInputMethodManagerWithInputMethods();

  MockInputMethodManagerWithInputMethods(
      const MockInputMethodManagerWithInputMethods&) = delete;
  MockInputMethodManagerWithInputMethods& operator=(
      const MockInputMethodManagerWithInputMethods&) = delete;

  ~MockInputMethodManagerWithInputMethods() override;

  void AddInputMethod(const std::string& id,
                      const std::string& raw_layout,
                      const std::string& language_code);

 private:
  input_method::InputMethodDescriptors descriptors_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_L10N_UTIL_TEST_UTIL_H_
