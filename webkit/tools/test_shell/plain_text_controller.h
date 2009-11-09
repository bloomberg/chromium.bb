// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// PlainTextController class:
// Bound to a JavaScript window.plainText object using
// CppBoundClass::BindToJavascript(), this allows layout tests that are run in
// the test_shell to extract plain text from the DOM or the selection.
//
// The OSX reference file is in
// WebKit/WebKitTools/DumpRenderTree/PlainTextController.mm

#ifndef WEBKIT_TOOLS_TEST_SHELL_PLAIN_TEXT_CONTROLLER_H_
#define WEBKIT_TOOLS_TEST_SHELL_PLAIN_TEXT_CONTROLLER_H_

#include "build/build_config.h"
#include "webkit/glue/cpp_bound_class.h"

class TestShell;

class PlainTextController : public CppBoundClass {
 public:
  // Builds the property and method lists needed to bind this class to a JS
  // object.
  explicit PlainTextController(TestShell* shell);

  // JS callback methods.
  void plainText(const CppArgumentList& args, CppVariant* result);

  // Fall-back method: called if an unknown method is invoked.
  void fallbackMethod(const CppArgumentList& args, CppVariant* result);

 private:
  // Non-owning pointer.  The LayoutTestController is owned by the host.
  static TestShell* shell_;
};

#endif  // WEBKIT_TOOLS_TEST_SHELL_PLAIN_TEXT_CONTROLLER_H_

