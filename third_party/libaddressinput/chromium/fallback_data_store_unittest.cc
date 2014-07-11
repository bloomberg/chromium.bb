// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/libaddressinput/chromium/fallback_data_store.h"

#include <cstddef>
#include <ctime>
#include <string>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libaddressinput/src/cpp/src/util/json.h"
#include "third_party/libaddressinput/src/cpp/src/validating_util.h"

namespace autofill {

using i18n::addressinput::Json;
using i18n::addressinput::ValidatingUtil;

TEST(FallbackDataStore, Parsability) {
  std::string data;
  ASSERT_TRUE(FallbackDataStore::Get("data/US", &data));

  // Should be stale.
  EXPECT_FALSE(ValidatingUtil::UnwrapTimestamp(&data, time(NULL)));

  // Should be uncorrupted.
  EXPECT_TRUE(ValidatingUtil::UnwrapChecksum(&data));

  // Should be valid JSON.
  Json json;
  ASSERT_TRUE(json.ParseObject(data));

  // Should have a dictionary for "data/US", as this is aggregate data.
  std::string not_checked;
  EXPECT_FALSE(json.GetStringValueForKey("data/US", &not_checked));
  EXPECT_TRUE(json.HasDictionaryValueForKey("data/US"));
}

}  // namespace autofill
