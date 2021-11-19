// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/client/mock_report_queue_provider.h"

#include <memory>

#include "base/bind.h"
#include "base/no_destructor.h"
#include "components/reporting/client/mock_report_queue.h"
#include "components/reporting/client/report_queue.h"
#include "components/reporting/client/report_queue_configuration.h"
#include "components/reporting/client/report_queue_provider.h"
#include "components/reporting/storage/test_storage_module.h"
#include "report_queue_provider.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::_;
using testing::Return;

namespace reporting {

MockReportQueueProvider::MockReportQueueProvider()
    : ReportQueueProvider(base::BindRepeating(
          [](OnStorageModuleCreatedCallback storage_created_cb) {
            std::move(storage_created_cb)
                .Run(base::MakeRefCounted<test::TestStorageModule>());
          })) {}
MockReportQueueProvider::~MockReportQueueProvider() = default;

void MockReportQueueProvider::ExpectCreateNewQueueAndReturnNewMockQueue(
    int times) {
  EXPECT_CALL(*this, CreateNewQueue(_, _))
      .Times(times)
      .WillRepeatedly([](std::unique_ptr<ReportQueueConfiguration> config,
                         CreateReportQueueCallback cb) {
        std::move(cb).Run(std::make_unique<MockReportQueue>());
      });
}

}  // namespace reporting
