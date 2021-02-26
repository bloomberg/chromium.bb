// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/browser/autofill_util.h"

#include "base/strings/utf_string_conversions.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using AutofillUtilTest = PlatformTest;

using autofill::ExtractIDs;
using autofill::ExtractFillingResults;
using base::ASCIIToUTF16;

TEST_F(AutofillUtilTest, ExtractIDs) {
  std::vector<uint32_t> extracted_ids;
  NSString* valid_ids = @"[\"1\",\"2\"]";
  std::vector<uint32_t> expected_result = {1, 2};
  EXPECT_TRUE(ExtractIDs(valid_ids, &extracted_ids));
  EXPECT_EQ(expected_result, extracted_ids);

  extracted_ids.clear();
  NSString* empty_ids = @"[]";
  EXPECT_TRUE(ExtractIDs(empty_ids, &extracted_ids));
  EXPECT_TRUE(extracted_ids.empty());

  NSString* invalid_ids1 = @"[\"1\"\"2\"]";
  EXPECT_FALSE(ExtractIDs(invalid_ids1, &extracted_ids));
  NSString* invalid_ids2 = @"[1,2]";
  EXPECT_FALSE(ExtractIDs(invalid_ids2, &extracted_ids));
}

TEST_F(AutofillUtilTest, ExtractFillingResults) {
  std::map<uint32_t, base::string16> extracted_results;
  NSString* valid_results = @"{\"1\":\"username\",\"2\":\"adress\"}";
  std::map<uint32_t, base::string16> expected_result = {
      {1, ASCIIToUTF16("username")}, {2, ASCIIToUTF16("adress")}};
  EXPECT_TRUE(ExtractFillingResults(valid_results, &extracted_results));
  EXPECT_EQ(expected_result, extracted_results);

  extracted_results.clear();
  NSString* empty_results = @"{}";
  EXPECT_TRUE(ExtractFillingResults(empty_results, &extracted_results));
  EXPECT_TRUE(extracted_results.empty());

  NSString* invalid_results1 = @"{\"1\":\"username\"\"2\":\"adress\"}";
  EXPECT_FALSE(ExtractFillingResults(invalid_results1, &extracted_results));
  NSString* invalid_results2 = @"{\"1\":\"username\"\"2\":100}";
  EXPECT_FALSE(ExtractFillingResults(invalid_results2, &extracted_results));
}
