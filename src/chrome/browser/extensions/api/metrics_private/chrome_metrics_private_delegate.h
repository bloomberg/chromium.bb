// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_METRICS_PRIVATE_CHROME_METRICS_PRIVATE_DELEGATE_H_
#define CHROME_BROWSER_EXTENSIONS_API_METRICS_PRIVATE_CHROME_METRICS_PRIVATE_DELEGATE_H_

#include "base/macros.h"
#include "extensions/browser/api/metrics_private/metrics_private_delegate.h"

namespace extensions {

class ChromeMetricsPrivateDelegate : public MetricsPrivateDelegate {
 public:
  ChromeMetricsPrivateDelegate() {}
  ~ChromeMetricsPrivateDelegate() override {}

  // MetricsPrivateDelegate:
  bool IsCrashReportingEnabled() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeMetricsPrivateDelegate);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_METRICS_PRIVATE_CHROME_METRICS_PRIVATE_DELEGATE_H_
