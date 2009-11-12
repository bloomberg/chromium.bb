// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the definition for PlainTextController.

#include "webkit/tools/test_shell/plain_text_controller.h"
#include "webkit/tools/test_shell/test_shell.h"

#include "third_party/WebKit/WebKit/chromium/public/WebBindings.h"
#include "third_party/WebKit/WebKit/chromium/public/WebRange.h"
#include "third_party/WebKit/WebKit/chromium/public/WebString.h"

using WebKit::WebBindings;
using WebKit::WebRange;
using WebKit::WebString;

TestShell* PlainTextController::shell_ = NULL;

PlainTextController::PlainTextController(TestShell* shell) {
  // Set static shell_ variable.
  if (!shell_)
    shell_ = shell;

  // Initialize the map that associates methods of this class with the names
  // they will use when called by JavaScript.  The actual binding of those
  // names to their methods will be done by calling BindToJavaScript() (defined
  // by CppBoundClass, the parent to EventSendingController).
  BindMethod("plainText", &PlainTextController::plainText);

  // The fallback method is called when an unknown method is invoked.
  BindFallbackMethod(&PlainTextController::fallbackMethod);
}

void PlainTextController::plainText(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();

  if (args.size() < 1 || !args[0].isObject())
    return;

  // Check that passed-in object is, in fact, a range.
  NPObject* npobject = NPVARIANT_TO_OBJECT(args[0]);
  if (!npobject)
    return;
  WebRange range;
  if (!WebBindings::getRange(npobject, &range))
    return;

  // Extract the text using the Range's text() method
  WebString text = range.toPlainText();
  result->Set(text.utf8());
}

void PlainTextController::fallbackMethod(
    const CppArgumentList& args, CppVariant* result) {
  std::wstring message(
      L"JavaScript ERROR: unknown method called on PlainTextController");
  if (!shell_->layout_test_mode()) {
    logging::LogMessage("CONSOLE:", 0).stream() << message;
  } else {
    printf("CONSOLE MESSAGE: %S\n", message.c_str());
  }
  result->SetNull();
}

