// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_DEVELOPER_CONSOLE_LOGGER_H_
#define COMPONENTS_PAYMENTS_CONTENT_DEVELOPER_CONSOLE_LOGGER_H_

#include "base/macros.h"
#include "components/payments/core/error_logger.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class WebContents;
}

namespace payments {

// Logs messages for web developers to the developer console.
class DeveloperConsoleLogger : public ErrorLogger,
                               public content::WebContentsObserver {
 public:
  explicit DeveloperConsoleLogger(content::WebContents* web_contents);
  ~DeveloperConsoleLogger() override;

  // ErrorLogger;
  void Warn(const std::string& warning_message) const override;
  void Error(const std::string& error_message) const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(DeveloperConsoleLogger);
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_DEVELOPER_CONSOLE_LOGGER_H_
