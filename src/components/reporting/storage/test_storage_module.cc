// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/storage/test_storage_module.h"

#include <utility>

#include "base/callback.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/storage/storage_module_interface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Invoke;
using ::testing::WithArg;

namespace reporting {
namespace test {

TestStorageModuleStrict::TestStorageModuleStrict() {
  ON_CALL(*this, AddRecord)
      .WillByDefault(Invoke(this, &TestStorageModule::AddRecordSuccessfully));
  ON_CALL(*this, Flush)
      .WillByDefault(
          WithArg<1>(Invoke([](base::OnceCallback<void(Status)> callback) {
            std::move(callback).Run(Status::StatusOK());
          })));
}

TestStorageModuleStrict::~TestStorageModuleStrict() = default;

Record TestStorageModuleStrict::record() const {
  EXPECT_TRUE(record_.has_value());
  return record_.value();
}

Priority TestStorageModuleStrict::priority() const {
  EXPECT_TRUE(priority_.has_value());
  return priority_.value();
}

void TestStorageModuleStrict::AddRecordSuccessfully(
    Priority priority,
    Record record,
    base::OnceCallback<void(Status)> callback) {
  record_ = std::move(record);
  priority_ = priority;
  std::move(callback).Run(Status::StatusOK());
}

}  // namespace test
}  // namespace reporting
