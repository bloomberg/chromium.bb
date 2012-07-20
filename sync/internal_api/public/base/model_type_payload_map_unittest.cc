// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/base/model_type_payload_map.h"

#include <string>

#include "base/base64.h"
#include "base/memory/scoped_ptr.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace {

using base::ExpectDictStringValue;

class ModelTypePayloadMapTest : public testing::Test {};

TEST_F(ModelTypePayloadMapTest, TypePayloadMapToSet) {
  ModelTypePayloadMap payloads;
  payloads[BOOKMARKS] = "bookmarkpayload";
  payloads[APPS] = "";

  const ModelTypeSet types(BOOKMARKS, APPS);
  EXPECT_TRUE(ModelTypePayloadMapToEnumSet(payloads).Equals(types));
}

TEST_F(ModelTypePayloadMapTest, TypePayloadMapToValue) {
  ModelTypePayloadMap payloads;
  std::string encoded;
  payloads[BOOKMARKS] = "bookmarkpayload";
  base::Base64Encode(payloads[BOOKMARKS], &encoded);
  payloads[APPS] = "";

  scoped_ptr<DictionaryValue> value(ModelTypePayloadMapToValue(payloads));
  EXPECT_EQ(2u, value->size());
  ExpectDictStringValue(encoded, *value, "Bookmarks");
  ExpectDictStringValue("", *value, "Apps");
  EXPECT_FALSE(value->HasKey("Preferences"));
}

TEST_F(ModelTypePayloadMapTest, CoalescePayloads) {
  ModelTypePayloadMap original;
  std::string empty_payload;
  std::string payload1 = "payload1";
  std::string payload2 = "payload2";
  std::string payload3 = "payload3";
  original[BOOKMARKS] = empty_payload;
  original[PASSWORDS] = payload1;
  original[AUTOFILL] = payload2;
  original[THEMES] = payload3;

  ModelTypePayloadMap update;
  update[BOOKMARKS] = empty_payload;  // Same.
  update[PASSWORDS] = empty_payload;  // Overwrite with empty.
  update[AUTOFILL] = payload1;        // Overwrite with non-empty.
  update[SESSIONS] = payload2;        // New.
  // Themes untouched.

  CoalescePayloads(&original, update);
  ASSERT_EQ(5U, original.size());
  EXPECT_EQ(empty_payload, original[BOOKMARKS]);
  EXPECT_EQ(payload1, original[PASSWORDS]);
  EXPECT_EQ(payload1, original[AUTOFILL]);
  EXPECT_EQ(payload2, original[SESSIONS]);
  EXPECT_EQ(payload3, original[THEMES]);
}

}  // namespace
}  // namespace syncer
