// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_TOOLS_TEST_SHELL_TEST_SHELL_DEVTOOLS_CALLARGS_H_
#define WEBKIT_TOOLS_TEST_SHELL_TEST_SHELL_DEVTOOLS_CALLARGS_H_

#include "base/basictypes.h"

#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"

class TestShellDevToolsCallArgs {
 public:
  explicit TestShellDevToolsCallArgs(const WebKit::WebString& data);

  TestShellDevToolsCallArgs(const TestShellDevToolsCallArgs& args);

  ~TestShellDevToolsCallArgs();

  static int calls_count() { return calls_count_; }

  WebKit::WebString data_;

 private:
  static int calls_count_;
};

#endif  // WEBKIT_TOOLS_TEST_SHELL_TEST_SHELL_DEVTOOLS_CALLARGS_H_
