/*
 * Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Error codes and data structures used to report errors when loading a nexe.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_PLUGIN_ERROR_H
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_PLUGIN_ERROR_H

#include <string>

namespace plugin {

// These error codes are reported via UMA so, if you edit them:
// 1) make sure you understand UMA, first.
// 2) update src/tools/histograms/histograms.xml in
//  svn://svn.chromium.org/chrome-internal/trunk/src-internal
// Values are explicitly specified to make sure they don't shift around when
// edited, and also to make reading about:histograms easier.
enum PluginErrorCode {
  ERROR_LOAD_SUCCESS = 0,
  ERROR_LOAD_ABORTED = 1,
  ERROR_UNKNOWN = 2,
  ERROR_MANIFEST_RESOLVE_URL = 3,
  ERROR_MANIFEST_LOAD_URL = 4,
  ERROR_MANIFEST_STAT = 5,
  ERROR_MANIFEST_TOO_LARGE = 6,
  ERROR_MANIFEST_OPEN = 7,
  ERROR_MANIFEST_MEMORY_ALLOC = 8,
  ERROR_MANIFEST_READ = 9,
  ERROR_MANIFEST_PARSING = 10,
  ERROR_MANIFEST_SCHEMA_VALIDATE = 11,
  ERROR_MANIFEST_GET_NEXE_URL = 12,
  ERROR_NEXE_LOAD_URL = 13,
  ERROR_NEXE_ORIGIN_PROTOCOL = 14,
  ERROR_NEXE_FH_DUP = 15,
  ERROR_NEXE_STAT = 16,
  ERROR_ELF_CHECK_IO = 17,
  ERROR_ELF_CHECK_FAIL = 18,
  ERROR_SEL_LDR_INIT = 19,
  ERROR_SEL_LDR_CREATE_LAUNCHER = 20,
  ERROR_SEL_LDR_FD = 21,
  ERROR_SEL_LDR_LAUNCH = 22,
  // Deprecated, safe to reuse the # because never logged in UMA.
  // ERROR_SEL_LDR_COMMUNICATION = 23,
  ERROR_SEL_LDR_SEND_NEXE = 24,
  ERROR_SEL_LDR_HANDLE_PASSING = 25,
  ERROR_SEL_LDR_START_MODULE = 26,
  ERROR_SEL_LDR_START_STATUS = 27,
  ERROR_SRPC_CONNECTION_FAIL = 28,
  ERROR_START_PROXY_CHECK_PPP = 29,
  ERROR_START_PROXY_ALLOC = 30,
  ERROR_START_PROXY_MODULE = 31,
  ERROR_START_PROXY_INSTANCE = 32,
  ERROR_SEL_LDR_COMMUNICATION_CMD_CHANNEL = 33,
  ERROR_SEL_LDR_COMMUNICATION_REV_SETUP = 34,
  ERROR_SEL_LDR_COMMUNICATION_WRAPPER = 35,
  ERROR_SEL_LDR_COMMUNICATION_REV_SERVICE = 36,
  ERROR_START_PROXY_CRASH = 37,
  // If you add a code, read the enum comment above on how to update histograms.
  ERROR_MAX
};

class ErrorInfo {
 public:
  ErrorInfo() {
    Reset();
  }

  void Reset() {
    SetReport(ERROR_UNKNOWN, "");
  }

  void SetReport(PluginErrorCode error_code, const std::string& message) {
    error_code_ = error_code;
    message_ = message;
  }

  PluginErrorCode error_code() const {
    return error_code_;
  }

  void PrependMessage(const std::string& prefix) {
    message_ = prefix + message_;
  }

  const std::string& message() const {
    return message_;
  }

 private:
  PluginErrorCode error_code_;
  std::string message_;
  NACL_DISALLOW_COPY_AND_ASSIGN(ErrorInfo);
};

}  // namespace plugin

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_PLUGIN_ERROR_H
