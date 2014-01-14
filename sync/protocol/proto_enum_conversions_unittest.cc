// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Keep this file in sync with the .proto files in this directory.

#include "sync/protocol/proto_enum_conversions.h"

#include <string>

#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace {

class ProtoEnumConversionsTest : public testing::Test {
};

template <class T>
void TestEnumStringFunction(const char* (*enum_string_fn)(T),
                            int enum_min, int enum_max) {
  for (int i = enum_min; i <= enum_max; ++i) {
    const std::string& str = enum_string_fn(static_cast<T>(i));
    EXPECT_FALSE(str.empty());
  }
}

TEST_F(ProtoEnumConversionsTest, GetAppListItemTypeString) {
  TestEnumStringFunction(
      GetAppListItemTypeString,
      sync_pb::AppListSpecifics::AppListItemType_MIN,
      sync_pb::AppListSpecifics::AppListItemType_MAX);
}

TEST_F(ProtoEnumConversionsTest, GetBrowserTypeString) {
  TestEnumStringFunction(
      GetBrowserTypeString,
      sync_pb::SessionWindow::BrowserType_MIN,
      sync_pb::SessionWindow::BrowserType_MAX);
}

TEST_F(ProtoEnumConversionsTest, GetPageTransitionString) {
  TestEnumStringFunction(
      GetPageTransitionString,
      sync_pb::SyncEnums::PageTransition_MIN,
      sync_pb::SyncEnums::PageTransition_MAX);
}

TEST_F(ProtoEnumConversionsTest, GetPageTransitionQualifierString) {
  TestEnumStringFunction(
      GetPageTransitionRedirectTypeString,
      sync_pb::SyncEnums::PageTransitionRedirectType_MIN,
      sync_pb::SyncEnums::PageTransitionRedirectType_MAX);
}

TEST_F(ProtoEnumConversionsTest, GetUpdatesSourceString) {
  TestEnumStringFunction(
      GetUpdatesSourceString,
      sync_pb::GetUpdatesCallerInfo::GetUpdatesSource_MIN,
      sync_pb::GetUpdatesCallerInfo::PERIODIC);
  TestEnumStringFunction(
      GetUpdatesSourceString,
      sync_pb::GetUpdatesCallerInfo::RETRY,
      sync_pb::GetUpdatesCallerInfo::GetUpdatesSource_MAX);
}

TEST_F(ProtoEnumConversionsTest, GetResponseTypeString) {
  TestEnumStringFunction(
      GetResponseTypeString,
      sync_pb::CommitResponse::ResponseType_MIN,
      sync_pb::CommitResponse::ResponseType_MAX);
}

TEST_F(ProtoEnumConversionsTest, GetErrorTypeString) {
  // We have a gap, so we need to do two ranges.
  TestEnumStringFunction(
      GetErrorTypeString,
      sync_pb::SyncEnums::ErrorType_MIN,
      sync_pb::SyncEnums::MIGRATION_DONE);
  TestEnumStringFunction(
      GetErrorTypeString,
      sync_pb::SyncEnums::UNKNOWN,
      sync_pb::SyncEnums::ErrorType_MAX);

}

TEST_F(ProtoEnumConversionsTest, GetActionString) {
  TestEnumStringFunction(
      GetActionString,
      sync_pb::SyncEnums::Action_MIN,
      sync_pb::SyncEnums::Action_MAX);
}

}  // namespace
}  // namespace syncer
