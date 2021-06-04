// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/mock_signal_reporter.h"

namespace enterprise_connectors {

void DeviceTrustSignalReporterForTestBase::CreateMockReportQueueAndCallback(
    std::unique_ptr<QueueConfig> config,
    CreateQueueCallback create_queue_cb) {
  mock_queue_ = new testing::StrictMock<reporting::MockReportQueue>();
  std::move(create_queue_cb)
      .Run({std::unique_ptr<reporting::ReportQueue>(mock_queue_)});
}

void DeviceTrustSignalReporterForTestBase::FailCreateReportQueueAndCallback(
    std::unique_ptr<QueueConfig> config,
    CreateQueueCallback create_queue_cb) {
  std::move(create_queue_cb)
      .Run(reporting::Status(reporting::error::INTERNAL,
                             "Mocked ReportQueue creation failure for tests"));
}

policy::DMToken DeviceTrustSignalReporterForTestBase::GetDmToken() const {
  return policy::DMToken::CreateValidTokenForTesting("dummy_token");
}

MockDeviceTrustSignalReporter::MockDeviceTrustSignalReporter() = default;
MockDeviceTrustSignalReporter::~MockDeviceTrustSignalReporter() = default;

void MockDeviceTrustSignalReporter::PostCreateReportQueueTask(
    std::unique_ptr<QueueConfig> config,
    CreateQueueCallback create_queue_cb) {
  CreateMockReportQueueAndCallback(std::move(config),
                                   std::move(create_queue_cb));
}

}  // namespace enterprise_connectors
