// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VERSION_UI_VERSION_HANDLER_HELPER_H_
#define COMPONENTS_VERSION_UI_VERSION_HANDLER_HELPER_H_

#include <memory>

namespace base {
class Value;
}

namespace version_ui {

// Returns the list of variations to be displayed on the chrome:://version page.
std::unique_ptr<base::Value> GetVariationsList();

// Returns the variations information in command line format to be displayed on
// the chrome:://version page.
base::Value GetVariationsCommandLineAsValue();

}  // namespace version_ui

#endif  // COMPONENTS_VERSION_UI_VERSION_HANDLER_HELPER_H_
