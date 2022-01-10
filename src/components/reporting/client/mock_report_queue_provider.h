// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_CLIENT_MOCK_REPORT_QUEUE_PROVIDER_H_
#define COMPONENTS_REPORTING_CLIENT_MOCK_REPORT_QUEUE_PROVIDER_H_

#include <memory>

#include "base/task/sequenced_task_runner.h"
#include "components/reporting/client/report_queue.h"
#include "components/reporting/client/report_queue_configuration.h"
#include "components/reporting/client/report_queue_provider.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace reporting {

class MockReportQueueProvider : public ReportQueueProvider {
 public:
  MockReportQueueProvider();
  MockReportQueueProvider(const MockReportQueueProvider&) = delete;
  MockReportQueueProvider& operator=(const MockReportQueueProvider&) = delete;
  ~MockReportQueueProvider() override;

  // This method will make sure - by mocking - that CreateQueue on the provider
  // always returns a new std::unique_ptr<MockReportQueue> to simulate the
  // original behaviour. Note times is also added to be expected so you should
  // know how often you expect this method to be called.
  void ExpectCreateNewQueueAndReturnNewMockQueue(size_t times);

  // This method will make sure - by mocking - that CreateNewSpeculativeQueue on
  // the provider always returns a new std::unique_ptr<MockReportQueue,
  // base::OnTaskRunnerDeleter> on a sequenced task runner to simulate the
  // original behaviour. Note times is also added to be expected so you should
  // know how often you expect this method to be called.
  void ExpectCreateNewSpeculativeQueueAndReturnNewMockQueue(size_t times);

  MOCK_METHOD(void,
              CreateNewQueue,
              (std::unique_ptr<ReportQueueConfiguration> config,
               CreateReportQueueCallback cb),
              (override));

  MOCK_METHOD(
      (StatusOr<std::unique_ptr<ReportQueue, base::OnTaskRunnerDeleter>>),
      CreateNewSpeculativeQueue,
      (),
      (override));

  MOCK_METHOD(void, OnInitCompleted, (), ());

  MOCK_METHOD(void,
              ConfigureReportQueue,
              (std::unique_ptr<ReportQueueConfiguration> configuration,
               ReportQueueProvider::ReportQueueConfiguredCallback callback),
              (override));

 private:
  scoped_refptr<StorageModuleInterface> storage_;
};

}  // namespace reporting

#endif  // COMPONENTS_REPORTING_CLIENT_MOCK_REPORT_QUEUE_PROVIDER_H_
