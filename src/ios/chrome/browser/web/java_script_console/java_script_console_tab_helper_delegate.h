// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_JAVA_SCRIPT_CONSOLE_JAVA_SCRIPT_CONSOLE_TAB_HELPER_DELEGATE_H_
#define IOS_CHROME_BROWSER_WEB_JAVA_SCRIPT_CONSOLE_JAVA_SCRIPT_CONSOLE_TAB_HELPER_DELEGATE_H_

struct JavaScriptConsoleMessage;

class JavaScriptConsoleTabHelperDelegate {
 public:
  // Called when a JavaScript message has been logged.
  virtual void DidReceiveConsoleMessage(
      const JavaScriptConsoleMessage& message) = 0;

  virtual ~JavaScriptConsoleTabHelperDelegate() {}
};

#endif  // IOS_CHROME_BROWSER_WEB_JAVA_SCRIPT_CONSOLE_JAVA_SCRIPT_CONSOLE_TAB_HELPER_DELEGATE_H_
