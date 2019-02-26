// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CORE_ERROR_LOGGER_H_
#define COMPONENTS_PAYMENTS_CORE_ERROR_LOGGER_H_

#include <string>

#include "base/macros.h"

namespace payments {

// Logs messages to stderr. See DeveloperConsoleLogger for an implementation
// that logs messages to the developer console instead, which is more useful for
// web developers.
//
// Sample usage:
//
//   ErrorLogger log;
//   log.Warn("Something's wrong.");
class ErrorLogger {
 public:
  ErrorLogger();
  virtual ~ErrorLogger();

  // Disables logs for tests to keep test output clean.
  void DisableInTest();

  // Warn the web developer of a potential problem, e.g., "Could not find icons
  // for the payment handler, so the user experience will be degraded."
  virtual void Warn(const std::string& warning_message) const;

  // Let the web developer know that an error has been encountered, e.g.,
  // "Invalid format for 'serviceworker' field."
  virtual void Error(const std::string& error_message) const;

 protected:
  bool enabled_ = true;

 private:
  DISALLOW_COPY_AND_ASSIGN(ErrorLogger);
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CORE_ERROR_LOGGER_H_
