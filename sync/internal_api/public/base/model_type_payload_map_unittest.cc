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
  syncer::ModelTypePayloadMap original;
  std::string empty_payload;
  std::string payload1 = "payload1";
  std::string payload2 = "payload2";
  std::string payload3 = "payload3";
  original[syncer::BOOKMARKS] = empty_payload;
  original[syncer::PASSWORDS] = payload1;
  original[syncer::AUTOFILL] = payload2;
  original[syncer::THEMES] = payload3;

  syncer::ModelTypePayloadMap update;
  update[syncer::BOOKMARKS] = empty_payload;  // Same.
  update[syncer::PASSWORDS] = empty_payload;  // Overwrite with empty.
  update[syncer::AUTOFILL] = payload1;        // Overwrite with non-empty.
  update[syncer::SESSIONS] = payload2;        // New.
  // Themes untouched.

  CoalescePayloads(&original, update);
  ASSERT_EQ(5U, original.size());
  EXPECT_EQ(empty_payload, original[syncer::BOOKMARKS]);
  EXPECT_EQ(payload1, original[syncer::PASSWORDS]);
  EXPECT_EQ(payload1, original[syncer::AUTOFILL]);
  EXPECT_EQ(payload2, original[syncer::SESSIONS]);
  EXPECT_EQ(payload3, original[syncer::THEMES]);
}

}  // namespace
}  // namespace syncer
