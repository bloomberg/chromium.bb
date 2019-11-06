// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_INPUT_METHOD_EVENT_ROUTER_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_INPUT_METHOD_EVENT_ROUTER_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "ui/base/ime/chromeos/input_method_manager.h"

namespace content {
class BrowserContext;
}

namespace chromeos {

// Event router class for the input method events.
class ExtensionInputMethodEventRouter
    : public input_method::InputMethodManager::Observer {
 public:
  explicit ExtensionInputMethodEventRouter(content::BrowserContext* context);
  ~ExtensionInputMethodEventRouter() override;

  // Implements input_method::InputMethodManager::Observer:
  void InputMethodChanged(input_method::InputMethodManager* manager,
                          Profile* profile,
                          bool show_message) override;

 private:
  content::BrowserContext* context_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionInputMethodEventRouter);
};

}  // namespace chromeos
#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_INPUT_METHOD_EVENT_ROUTER_H_
