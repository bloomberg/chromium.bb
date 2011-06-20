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
  ERROR_NEXE_LOAD_URL,
  ERROR_NEXE_STAT,
  ERROR_SEL_LDR_INIT
};

class ErrorInfo {
 public:
  ErrorInfo() {
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

  // TODO(ncbray): remove once refactorings have been performed.
  // http://code.google.com/p/nativeclient/issues/detail?id=1923
  std::string* message_ptr() {
    return &message_;
  }

 private:
  PluginErrorCode type_;
  std::string message_;
  NACL_DISALLOW_COPY_AND_ASSIGN(ErrorInfo);
};

}  // namespace plugin

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_PLUGIN_ERROR_H
