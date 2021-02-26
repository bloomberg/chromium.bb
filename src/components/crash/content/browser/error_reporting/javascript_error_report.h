// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRASH_CONTENT_BROWSER_ERROR_REPORTING_JAVASCRIPT_ERROR_REPORT_H_
#define COMPONENTS_CRASH_CONTENT_BROWSER_ERROR_REPORTING_JAVASCRIPT_ERROR_REPORT_H_

#include <string>

#include "base/component_export.h"
#include "base/optional.h"

enum class WindowType {
  // No browser found, thus no window exists.
  kNoBrowser,
  // Valid window types.
  kRegularTabbed,
  kWebApp,
  kSystemWebApp,
};

// A report about a JavaScript error that we might want to send back to Google
// so it can be fixed. Fill in the fields and then call
// SendJavaScriptErrorReport.
struct COMPONENT_EXPORT(JS_ERROR_REPORTING) JavaScriptErrorReport {
  JavaScriptErrorReport();
  JavaScriptErrorReport(const JavaScriptErrorReport& rhs);
  JavaScriptErrorReport(JavaScriptErrorReport&& rhs) noexcept;
  ~JavaScriptErrorReport();
  JavaScriptErrorReport& operator=(const JavaScriptErrorReport& rhs);
  JavaScriptErrorReport& operator=(JavaScriptErrorReport&& rhs) noexcept;

  // The error message. Must be present
  std::string message;

  // URL where the error occurred. Must be the full URL, containing the protocol
  // (e.g. http://www.example.com).
  std::string url;

  // Name of the product where the error occurred. If empty, use the product
  // variant of Chrome that is hosting the extension. (e.g. "Chrome" or
  // "Chrome_ChromeOS").
  std::string product;

  // Version of the product where the error occurred. If empty, use the version
  // of Chrome that is hosting the extension (e.g. "73.0.3683.75").
  std::string version;

  // Line number where the error occurred. Not sent if not present.
  base::Optional<int> line_number;

  // Column number where the error occurred. Not sent if not present.
  base::Optional<int> column_number;

  // String containing the stack trace for the error. Not sent if not present.
  base::Optional<std::string> stack_trace;

  // String containing the application locale. Not sent if not present.
  base::Optional<std::string> app_locale;

  // Uptime of the renderer process in milliseconds. 0 if the callee
  // |web_contents| is null (shouldn't really happen as this is caled from a JS
  // context) or the renderer process doesn't exist (possible due to termination
  // / failure to start).
  int renderer_process_uptime_ms = 0;

  // The window type of the JS context that reported this error.
  base::Optional<WindowType> window_type;

  // If true (the default), send this report to the production server. If false,
  // send to the staging server. This should be set to false for errors from
  // tests and dev builds, and true for real (end-user) errors.
  bool send_to_production_servers = true;
};

#endif  // COMPONENTS_CRASH_CONTENT_BROWSER_ERROR_REPORTING_JAVASCRIPT_ERROR_REPORT_H_
