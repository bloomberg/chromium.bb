// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/protocol/proto_enum_conversions.h"

#include <string>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace {

// WARNING: Keep this file in sync with the .proto files in this directory.

using ::testing::Not;
using ::testing::StrEq;

// Iterates through the enum values, checking their string version is non-empty.
// The T##_IsValid() check is needed because some enums have deprecated values,
// so they have gaps in their numeric range.
#define TestEnumStringsNonEmpty(T)                                       \
  for (int i = T##_MIN; i <= T##_MAX; ++i) {                             \
    if (T##_IsValid(i)) {                                                \
      EXPECT_THAT(ProtoEnumToString(static_cast<T>(i)), Not(StrEq(""))); \
    }                                                                    \
  }

TEST(ProtoEnumConversionsTest, GetAppListItemTypeString) {
  TestEnumStringsNonEmpty(sync_pb::AppListSpecifics::AppListItemType);
}

TEST(ProtoEnumConversionsTest, GetBrowserTypeString) {
  TestEnumStringsNonEmpty(sync_pb::SessionWindow::BrowserType);
}

TEST(ProtoEnumConversionsTest, GetPageTransitionString) {
  TestEnumStringsNonEmpty(sync_pb::SyncEnums::PageTransition);
}

TEST(ProtoEnumConversionsTest, GetPageTransitionQualifierString) {
  TestEnumStringsNonEmpty(sync_pb::SyncEnums::PageTransitionRedirectType);
}

TEST(ProtoEnumConversionsTest, GetWifiConfigurationSecurityTypeString) {
  TestEnumStringsNonEmpty(sync_pb::WifiConfigurationSpecifics::SecurityType);
}

TEST(ProtoEnumConversionsTest,
     GetWifiConfigurationAutomaticallyConnectOptionString) {
  TestEnumStringsNonEmpty(
      sync_pb::WifiConfigurationSpecifics::AutomaticallyConnectOption);
}

TEST(ProtoEnumConversionsTest, GetWifiConfigurationIsPreferredOptionString) {
  TestEnumStringsNonEmpty(
      sync_pb::WifiConfigurationSpecifics::IsPreferredOption);
}

TEST(ProtoEnumConversionsTest, GetWifiConfigurationMeteredOptionString) {
  TestEnumStringsNonEmpty(sync_pb::WifiConfigurationSpecifics::MeteredOption);
}

TEST(ProtoEnumConversionsTest, GetWifiConfigurationProxyOptionString) {
  TestEnumStringsNonEmpty(
      sync_pb::WifiConfigurationSpecifics::ProxyConfiguration::ProxyOption);
}

TEST(ProtoEnumConversionsTest, GetUpdatesSourceString) {
  TestEnumStringsNonEmpty(sync_pb::GetUpdatesCallerInfo::GetUpdatesSource);
}

TEST(ProtoEnumConversionsTest, GetUpdatesOriginString) {
  TestEnumStringsNonEmpty(sync_pb::SyncEnums::GetUpdatesOrigin);
}

TEST(ProtoEnumConversionsTest, GetResponseTypeString) {
  TestEnumStringsNonEmpty(sync_pb::CommitResponse::ResponseType);
}

TEST(ProtoEnumConversionsTest, GetErrorTypeString) {
  TestEnumStringsNonEmpty(sync_pb::SyncEnums::ErrorType);
}

TEST(ProtoEnumConversionsTest, GetActionString) {
  TestEnumStringsNonEmpty(sync_pb::SyncEnums::Action);
}

TEST(ProtoEnumConversionsTest, GetConsentStatusString) {
  TestEnumStringsNonEmpty(sync_pb::UserConsentTypes::ConsentStatus);
}

}  // namespace
}  // namespace syncer
