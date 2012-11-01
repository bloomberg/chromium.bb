// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/sessions/sync_source_info.h"

#include "base/test/values_test_util.h"
#include "base/values.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/base/model_type_invalidation_map.h"
#include "sync/protocol/sync.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace sessions {
namespace {

using base::ExpectDictDictionaryValue;
using base::ExpectDictStringValue;

class SyncSourceInfoTest : public testing::Test {};

TEST_F(SyncSourceInfoTest, SyncSourceInfoToValue) {
  sync_pb::GetUpdatesCallerInfo::GetUpdatesSource updates_source =
      sync_pb::GetUpdatesCallerInfo::PERIODIC;
  ModelTypeInvalidationMap types;
  types[PREFERENCES].payload = "preferencespayload";
  types[EXTENSIONS].payload = "";
  scoped_ptr<DictionaryValue> expected_types_value(
      ModelTypeInvalidationMapToValue(types));

  SyncSourceInfo source_info(updates_source, types);

  scoped_ptr<DictionaryValue> value(source_info.ToValue());
  EXPECT_EQ(2u, value->size());
  ExpectDictStringValue("PERIODIC", *value, "updatesSource");
  ExpectDictDictionaryValue(*expected_types_value, *value, "types");
}

}  // namespace syncer
}  // namespace sessions
}  // namespace
