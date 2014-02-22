// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/api/sync_data.h"

#include <string>

#include "base/time/time.h"
#include "sync/protocol/sync.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

using std::string;

namespace syncer {

namespace {

const string kSyncTag = "3984729834";
const ModelType kDatatype = syncer::PREFERENCES;
const string kNonUniqueTitle = "my preference";
const int64 kId = 439829;
const base::Time kLastModifiedTime = base::Time();

typedef testing::Test SyncDataTest;

TEST_F(SyncDataTest, NoArgCtor) {
  SyncData data;
  EXPECT_FALSE(data.IsValid());
}

TEST_F(SyncDataTest, CreateLocalDelete) {
  SyncData data = SyncData::CreateLocalDelete(kSyncTag, kDatatype);
  EXPECT_TRUE(data.IsValid());
  EXPECT_TRUE(data.IsLocal());
  EXPECT_EQ(kSyncTag, data.GetTag());
  EXPECT_EQ(kDatatype, data.GetDataType());
}

TEST_F(SyncDataTest, CreateLocalData) {
  sync_pb::EntitySpecifics specifics;
  specifics.mutable_preference();
  SyncData data =
      SyncData::CreateLocalData(kSyncTag, kNonUniqueTitle, specifics);
  EXPECT_TRUE(data.IsValid());
  EXPECT_TRUE(data.IsLocal());
  EXPECT_EQ(kSyncTag, data.GetTag());
  EXPECT_EQ(kDatatype, data.GetDataType());
  EXPECT_EQ(kNonUniqueTitle, data.GetTitle());
  EXPECT_TRUE(data.GetSpecifics().has_preference());
}

TEST_F(SyncDataTest, CreateRemoteData) {
  sync_pb::EntitySpecifics specifics;
  specifics.mutable_preference();
  SyncData data = SyncData::CreateRemoteData(kId, specifics, kLastModifiedTime);
  EXPECT_TRUE(data.IsValid());
  EXPECT_FALSE(data.IsLocal());
  EXPECT_EQ(kId, data.GetRemoteId());
  EXPECT_EQ(kLastModifiedTime, data.GetRemoteModifiedTime());
  EXPECT_TRUE(data.GetSpecifics().has_preference());
}

}  // namespace

}  // namespace syncer
