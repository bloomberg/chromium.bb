// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/values.h"
#include "components/sync/driver/sync_internals_util.h"
#include "components/sync/driver/test_sync_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace sync_ui_util {
namespace {

TEST(SyncUIUtilTestAbout, ConstructAboutInformationWithUnrecoverableErrorTest) {
  TestSyncService service;
  service.SetDisableReasons(
      syncer::SyncService::DISABLE_REASON_UNRECOVERABLE_ERROR);

  std::unique_ptr<base::DictionaryValue> strings(ConstructAboutInformation(
      IncludeSensitiveData(true), &service, std::string()));

  EXPECT_TRUE(strings->HasKey("unrecoverable_error_detected"));
}

}  // namespace
}  // namespace sync_ui_util
}  // namespace syncer
