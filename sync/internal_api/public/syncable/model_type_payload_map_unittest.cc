// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/syncable/model_type_payload_map.h"

#include <string>

#include "base/base64.h"
#include "base/memory/scoped_ptr.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace syncable {
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

}  // namespace
}  // namespace syncable
}  // namespace syncer
