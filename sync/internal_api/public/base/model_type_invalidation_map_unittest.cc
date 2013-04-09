// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/base/model_type_invalidation_map.h"

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace {

using base::ExpectDictStringValue;

class ModelTypeInvalidationMapTest : public testing::Test {};

TEST_F(ModelTypeInvalidationMapTest, TypeInvalidationMapToSet) {
  ModelTypeInvalidationMap states;
  states[BOOKMARKS].payload = "bookmarkpayload";
  states[APPS].payload = "";

  const ModelTypeSet types(BOOKMARKS, APPS);
  EXPECT_TRUE(ModelTypeInvalidationMapToSet(states).Equals(types));
}

TEST_F(ModelTypeInvalidationMapTest, TypeInvalidationMapToValue) {
  ModelTypeInvalidationMap states;
  states[BOOKMARKS].payload = "bookmarkpayload";
  states[APPS].payload = "";

  scoped_ptr<DictionaryValue> value(ModelTypeInvalidationMapToValue(states));
  EXPECT_EQ(2u, value->size());
  ExpectDictStringValue(states[BOOKMARKS].payload, *value, "Bookmarks");
  ExpectDictStringValue("", *value, "Apps");
  EXPECT_FALSE(value->HasKey("Preferences"));
}

TEST_F(ModelTypeInvalidationMapTest, CoalesceStates) {
  ModelTypeInvalidationMap original;
  std::string empty_payload;
  std::string payload1 = "payload1";
  std::string payload2 = "payload2";
  std::string payload3 = "payload3";
  original[BOOKMARKS].payload = empty_payload;
  original[PASSWORDS].payload = payload1;
  original[AUTOFILL].payload = payload2;
  original[THEMES].payload = payload3;

  ModelTypeInvalidationMap update;
  update[BOOKMARKS].payload = empty_payload;  // Same.
  update[PASSWORDS].payload = empty_payload;  // Overwrite with empty.
  update[AUTOFILL].payload = payload1;        // Overwrite with non-empty.
  update[SESSIONS].payload = payload2;        // New.
  // Themes untouched.

  CoalesceStates(update, &original);
  ASSERT_EQ(5U, original.size());
  EXPECT_EQ(empty_payload, original[BOOKMARKS].payload);
  EXPECT_EQ(payload1, original[PASSWORDS].payload);
  EXPECT_EQ(payload1, original[AUTOFILL].payload);
  EXPECT_EQ(payload2, original[SESSIONS].payload);
  EXPECT_EQ(payload3, original[THEMES].payload);
}

}  // namespace
}  // namespace syncer
