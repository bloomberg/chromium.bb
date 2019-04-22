// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_JAVA_SCRIPT_CONSOLE_JAVA_SCRIPT_CONSOLE_MESSAGE_H_
#define IOS_CHROME_BROWSER_WEB_JAVA_SCRIPT_CONSOLE_JAVA_SCRIPT_CONSOLE_MESSAGE_H_

#include "base/macros.h"
#include "base/values.h"
#include "url/gurl.h"

// Wraps information from a received console message.
struct JavaScriptConsoleMessage {
 public:
  JavaScriptConsoleMessage();
  JavaScriptConsoleMessage(const JavaScriptConsoleMessage& other);
  JavaScriptConsoleMessage& operator=(JavaScriptConsoleMessage other);
  ~JavaScriptConsoleMessage();

  // The url of the frame which sent the message.
  GURL url;

  // The log level associated with the message. (From console.js, i.e. "log",
  // "debug", "info", "warn", "error")
  std::string level;

  // The message contents.
  std::unique_ptr<base::Value> message;

  DISALLOW_ASSIGN(JavaScriptConsoleMessage);
};

#endif  // IOS_CHROME_BROWSER_WEB_JAVA_SCRIPT_CONSOLE_JAVA_SCRIPT_CONSOLE_MESSAGE_H_
