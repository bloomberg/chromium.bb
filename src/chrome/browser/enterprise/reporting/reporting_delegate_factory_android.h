// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_REPORTING_REPORTING_DELEGATE_FACTORY_ANDROID_H_
#define CHROME_BROWSER_ENTERPRISE_REPORTING_REPORTING_DELEGATE_FACTORY_ANDROID_H_

#include "components/enterprise/browser/reporting/reporting_delegate_factory.h"

namespace enterprise_reporting {

// Android implementation of the reporting delegate factory. Creates android-
// specific delegates for the enterprise reporting classes.
class ReportingDelegateFactoryAndroid : public ReportingDelegateFactory {
 public:
  ReportingDelegateFactoryAndroid() = default;
  ReportingDelegateFactoryAndroid(const ReportingDelegateFactoryAndroid&) =
      delete;
  ReportingDelegateFactoryAndroid& operator=(
      const ReportingDelegateFactoryAndroid&) = delete;
  ~ReportingDelegateFactoryAndroid() override = default;

  // ReportingDelegateFactory implementation.
  std::unique_ptr<BrowserReportGenerator::Delegate>
  GetBrowserReportGeneratorDelegate() override;
  std::unique_ptr<ProfileReportGenerator::Delegate>
  GetProfileReportGeneratorDelegate() override;
  std::unique_ptr<ReportGenerator::Delegate> GetReportGeneratorDelegate()
      override;
  std::unique_ptr<ReportScheduler::Delegate> GetReportSchedulerDelegate()
      override;
  std::unique_ptr<RealTimeReportGenerator::Delegate>
  GetRealTimeReportGeneratorDelegate() override;
};

}  // namespace enterprise_reporting

#endif  // CHROME_BROWSER_ENTERPRISE_REPORTING_REPORTING_DELEGATE_FACTORY_ANDROID_H_
