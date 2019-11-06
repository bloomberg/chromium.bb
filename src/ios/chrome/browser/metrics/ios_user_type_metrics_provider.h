// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_METRICS_IOS_USER_TYPE_METRICS_PROVIDER_H_
#define IOS_CHROME_BROWSER_METRICS_IOS_USER_TYPE_METRICS_PROVIDER_H_

#include "base/macros.h"
#include "components/metrics/metrics_provider.h"

// IOSUserTypeMetricsProvider logs user type related metrics.
class IOSUserTypeMetricsProvider : public metrics::MetricsProvider {
 public:
  IOSUserTypeMetricsProvider();
  ~IOSUserTypeMetricsProvider() override;

  // metrics::MetricsDataProvider:
  void ProvideCurrentSessionData(
      metrics::ChromeUserMetricsExtension* uma_proto) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(IOSUserTypeMetricsProvider);
};

#endif  // IOS_CHROME_BROWSER_METRICS_IOS_USER_TYPE_METRICS_PROVIDER_H_
