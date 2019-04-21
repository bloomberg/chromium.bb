// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUGINS_PLUGIN_TEST_UTILS_H_
#define CHROME_BROWSER_PLUGINS_PLUGIN_TEST_UTILS_H_

#include <string>

#include "base/macros.h"
#include "base/strings/string_piece.h"

namespace content {
class WebContents;
}

class PluginTestUtils {
 public:
  // Runs the JavaScript |test_script|, which is provided 'plugin' as a variable
  // referencing the |element_id| element. Returns the string extracted from
  // window.domAutomationController.
  static std::string RunTestScript(base::StringPiece test_script,
                                   content::WebContents* contents,
                                   const std::string& element_id);

  // Blocks until the placeholder is ready.
  static void WaitForPlaceholderReady(content::WebContents* contents,
                                      const std::string& element_id);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(PluginTestUtils);
};

#endif  // CHROME_BROWSER_PLUGINS_PLUGIN_TEST_UTILS_H_
