/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
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

// These enums should be kept roughly in the order they occur during loading.
enum PluginErrorCode {
  ERROR_UNKNOWN = 0,
  ERROR_MANIFEST_RESOLVE_URL,
  ERROR_MANIFEST_LOAD_URL,
  ERROR_MANIFEST_STAT,
  ERROR_MANIFEST_TOO_LARGE,
  ERROR_MANIFEST_OPEN,
  ERROR_MANIFEST_MEMORY_ALLOC,
  ERROR_MANIFEST_READ,
  ERROR_MANIFEST_PARSING,
  ERROR_MANIFEST_SCHEMA_VALIDATE,
  ERROR_MANIFEST_GET_NEXE_URL,
  ERROR_NEXE_LOAD_URL,
  ERROR_NEXE_ORIGIN_PROTOCOL,
  ERROR_NEXE_FH_DUP,
  ERROR_NEXE_STAT,
  ERROR_ELF_CHECK_IO,
  ERROR_ELF_CHECK_FAIL,
  ERROR_SEL_LDR_INIT,
  ERROR_SEL_LDR_CREATE_LAUNCHER,
  ERROR_SEL_LDR_FD,
  ERROR_SEL_LDR_LAUNCH,
  ERROR_SEL_LDR_COMMUNICATION,
  ERROR_SEL_LDR_SEND_NEXE,
  ERROR_SEL_LDR_HANDLE_PASSING,
  ERROR_SEL_LDR_START_MODULE,
  ERROR_SEL_LDR_START_STATUS,
  ERROR_SRPC_CONNECTION_FAIL,
  ERROR_START_PROXY
};

class ErrorInfo {
 public:
  ErrorInfo() {
    Reset();
  }

  void Reset() {
    SetReport(ERROR_UNKNOWN, "");
  }

  void SetReport(const PluginErrorCode type, const std::string& message) {
    type_ = type;
    message_ = message;
  }

  void PrependMessage(const std::string& prefix) {
    message_ = prefix + message_;
  }

  const std::string& message() const {
    return message_;
  }

 private:
  PluginErrorCode type_;
  std::string message_;
  NACL_DISALLOW_COPY_AND_ASSIGN(ErrorInfo);
};

}  // namespace plugin

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_PLUGIN_ERROR_H
