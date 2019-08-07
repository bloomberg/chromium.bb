// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_EXTENSION_JS_BROWSER_TEST_H_
#define CHROME_TEST_BASE_EXTENSION_JS_BROWSER_TEST_H_

#include "base/callback_forward.h"
#include "base/macros.h"
#include "chrome/test/base/extension_load_waiter_one_shot.h"
#include "chrome/test/base/javascript_browser_test.h"

// A super class that handles javascript-based tests against an extension.
//
// See an example usage at
//chrome/browser/resources/chromeos/chromevox2/cvox2/background/background.extjs
class ExtensionJSBrowserTest : public JavaScriptBrowserTest {
 public:
  ExtensionJSBrowserTest();

  ~ExtensionJSBrowserTest() override;

 protected:
  // Waits for an extension to load; returns immediately if already loaded.
  void WaitForExtension(const char* extension_id, const base::Closure& load_cb);

  // Method required for js2gtest.
  // Runs |test_fixture|.|test_name| using the framework in test_api.js.
  bool RunJavascriptTestF(bool is_async,
                          const std::string& test_fixture,
                          const std::string& test_name);

 private:
  std::unique_ptr<ExtensionLoadWaiterOneShot> load_waiter_;
  bool libs_loaded_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionJSBrowserTest);
};

#endif  // CHROME_TEST_BASE_EXTENSION_JS_BROWSER_TEST_H_
