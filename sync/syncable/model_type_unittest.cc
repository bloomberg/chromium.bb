// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/base/model_type.h"

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "sync/protocol/sync.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace {

class ModelTypeTest : public testing::Test {};

TEST_F(ModelTypeTest, ModelTypeToValue) {
  for (int i = FIRST_REAL_MODEL_TYPE; i < MODEL_TYPE_COUNT; ++i) {
    ModelType model_type = ModelTypeFromInt(i);
    base::ExpectStringValue(ModelTypeToString(model_type),
                            ModelTypeToValue(model_type));
  }
  base::ExpectStringValue("Top-level folder",
                          ModelTypeToValue(TOP_LEVEL_FOLDER));
  base::ExpectStringValue("Unspecified",
                          ModelTypeToValue(UNSPECIFIED));
}

TEST_F(ModelTypeTest, ModelTypeFromValue) {
  for (int i = FIRST_REAL_MODEL_TYPE; i < MODEL_TYPE_COUNT; ++i) {
    ModelType model_type = ModelTypeFromInt(i);
    scoped_ptr<base::StringValue> value(ModelTypeToValue(model_type));
    EXPECT_EQ(model_type, ModelTypeFromValue(*value));
  }
}

TEST_F(ModelTypeTest, ModelTypeSetToValue) {
  const ModelTypeSet model_types(BOOKMARKS, APPS);

  scoped_ptr<base::ListValue> value(ModelTypeSetToValue(model_types));
  EXPECT_EQ(2u, value->GetSize());
  std::string types[2];
  EXPECT_TRUE(value->GetString(0, &types[0]));
  EXPECT_TRUE(value->GetString(1, &types[1]));
  EXPECT_EQ("Bookmarks", types[0]);
  EXPECT_EQ("Apps", types[1]);
}

TEST_F(ModelTypeTest, ModelTypeSetFromValue) {
  // Try empty set first.
  ModelTypeSet model_types;
  scoped_ptr<base::ListValue> value(ModelTypeSetToValue(model_types));
  EXPECT_TRUE(model_types.Equals(ModelTypeSetFromValue(*value)));

  // Now try with a few random types.
  model_types.Put(BOOKMARKS);
  model_types.Put(APPS);
  value.reset(ModelTypeSetToValue(model_types));
  EXPECT_TRUE(model_types.Equals(ModelTypeSetFromValue(*value)));
}

TEST_F(ModelTypeTest, IsRealDataType) {
  EXPECT_FALSE(IsRealDataType(UNSPECIFIED));
  EXPECT_FALSE(IsRealDataType(MODEL_TYPE_COUNT));
  EXPECT_FALSE(IsRealDataType(TOP_LEVEL_FOLDER));
  EXPECT_TRUE(IsRealDataType(FIRST_REAL_MODEL_TYPE));
  EXPECT_TRUE(IsRealDataType(BOOKMARKS));
  EXPECT_TRUE(IsRealDataType(APPS));
}

TEST_F(ModelTypeTest, IsProxyType) {
  EXPECT_FALSE(IsProxyType(BOOKMARKS));
  EXPECT_FALSE(IsProxyType(MODEL_TYPE_COUNT));
  EXPECT_TRUE(IsProxyType(PROXY_TABS));
}

// Make sure we can convert ModelTypes to and from specifics field
// numbers.
TEST_F(ModelTypeTest, ModelTypeToFromSpecificsFieldNumber) {
  ModelTypeSet protocol_types = ProtocolTypes();
  for (ModelTypeSet::Iterator iter = protocol_types.First(); iter.Good();
       iter.Inc()) {
    int field_number = GetSpecificsFieldNumberFromModelType(iter.Get());
    EXPECT_EQ(iter.Get(),
              GetModelTypeFromSpecificsFieldNumber(field_number));
  }
}

TEST_F(ModelTypeTest, ModelTypeOfInvalidSpecificsFieldNumber) {
  EXPECT_EQ(UNSPECIFIED, GetModelTypeFromSpecificsFieldNumber(0));
}

TEST_F(ModelTypeTest, ModelTypeHistogramMapping) {
  std::set<int> histogram_values;
  ModelTypeSet all_types = ModelTypeSet::All();
  for (ModelTypeSet::Iterator it = all_types.First(); it.Good(); it.Inc()) {
    SCOPED_TRACE(ModelTypeToString(it.Get()));
    int histogram_value = ModelTypeToHistogramInt(it.Get());

    EXPECT_TRUE(histogram_values.insert(histogram_value).second)
        << "Expected histogram values to be unique";

    // This is not necessary for the mapping to be valid, but most instances of
    // UMA_HISTOGRAM that use this mapping specify MODEL_TYPE_COUNT as the
    // maximum possible value.  If you break this assumption, you should update
    // those histograms.
    EXPECT_LT(histogram_value, MODEL_TYPE_COUNT);
  }
}

TEST_F(ModelTypeTest, ModelTypeSetFromString) {
  syncer::ModelTypeSet empty;
  syncer::ModelTypeSet one(BOOKMARKS);
  syncer::ModelTypeSet two(BOOKMARKS, TYPED_URLS);

  EXPECT_TRUE(
      empty.Equals(ModelTypeSetFromString(ModelTypeSetToString(empty))));
  EXPECT_TRUE(
      one.Equals(ModelTypeSetFromString(ModelTypeSetToString(one))));
  EXPECT_TRUE(
      two.Equals(ModelTypeSetFromString(ModelTypeSetToString(two))));
}

TEST_F(ModelTypeTest, DefaultFieldValues) {
  syncer::ModelTypeSet types = syncer::ProtocolTypes();
  for (ModelTypeSet::Iterator it = types.First(); it.Good(); it.Inc()) {
    SCOPED_TRACE(ModelTypeToString(it.Get()));

    sync_pb::EntitySpecifics specifics;
    syncer::AddDefaultFieldValue(it.Get(), &specifics);
    EXPECT_TRUE(specifics.IsInitialized());

    std::string tmp;
    EXPECT_TRUE(specifics.SerializeToString(&tmp));

    sync_pb::EntitySpecifics from_string;
    EXPECT_TRUE(from_string.ParseFromString(tmp));
    EXPECT_TRUE(from_string.IsInitialized());

    EXPECT_EQ(it.Get(), syncer::GetModelTypeFromSpecifics(from_string));
  }
}

}  // namespace
}  // namespace syncer
