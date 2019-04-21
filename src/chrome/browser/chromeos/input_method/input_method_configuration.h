// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_INPUT_METHOD_CONFIGURATION_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_INPUT_METHOD_CONFIGURATION_H_

#include "base/sequenced_task_runner.h"

namespace chromeos {
namespace input_method {

class InputMethodManager;

// Initializes the InputMethodManager. Must be called before any calls to
// GetInstance(). We explicitly initialize and shut down the global instance,
// rather than making it a Singleton, to ensure clean startup and shutdown.
void Initialize();

// Similar to Initialize(), but can inject an alternative
// InputMethodManager such as MockInputMethodManager for testing.
// The injected object will be owned by the internal pointer and deleted
// by Shutdown().
// TODO(nona): Remove this and use InputMethodManager::Initialize instead.
void InitializeForTesting(InputMethodManager* mock_manager);

// Disables the IME extension loading (e.g. for browser tests).
void DisableExtensionLoading();

// Destroys the global InputMethodManager instance.
void Shutdown();

}  // namespace input_method
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_INPUT_METHOD_CONFIGURATION_H_
