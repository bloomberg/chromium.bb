// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_baseline.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

using ::testing::ElementsAre;
using ::testing::ElementsAreArray;

class NGBaselineTest : public testing::Test {
 public:
  static NGBaselineRequest RequestFromTypeId(unsigned type_id) {
    return NGBaselineRequest::FromTypeId(type_id);
  }

  static Vector<NGBaselineRequest> ToList(NGBaselineRequestList requests) {
    Vector<NGBaselineRequest> list;
    for (const NGBaselineRequest request : requests)
      list.push_back(request);
    return list;
  }
};

struct NGBaselineRequestListTestData {
  unsigned count;
  unsigned type_ids[4];
} baseline_request_list_test_data[] = {
    {0, {}},        {1, {0}},       {1, {1}},       {1, {2}},
    {1, {3}},       {2, {0, 1}},    {2, {0, 2}},    {2, {1, 3}},
    {3, {0, 1, 2}}, {3, {0, 2, 3}}, {3, {1, 2, 3}}, {4, {0, 1, 2, 3}},
};

class NGBaselineRequestListDataTest
    : public NGBaselineTest,
      public testing::WithParamInterface<NGBaselineRequestListTestData> {};

INSTANTIATE_TEST_SUITE_P(NGBaselineTest,
                         NGBaselineRequestListDataTest,
                         testing::ValuesIn(baseline_request_list_test_data));

TEST_P(NGBaselineRequestListDataTest, Data) {
  const auto& data = GetParam();
  NGBaselineRequestList requests;
  Vector<NGBaselineRequest> expected;
  for (unsigned i = 0; i < data.count; i++) {
    NGBaselineRequest request = RequestFromTypeId(data.type_ids[i]);
    requests.push_back(request);
    expected.push_back(request);
  }

  EXPECT_EQ(requests.IsEmpty(), !data.count);

  Vector<NGBaselineRequest> actual = ToList(requests);
  EXPECT_THAT(actual, expected);
}

TEST_F(NGBaselineTest, BaselineList) {
  NGBaselineList list;
  EXPECT_TRUE(list.IsEmpty());

  NGBaselineRequest request(NGBaselineAlgorithmType::kFirstLine,
                            FontBaseline::kAlphabeticBaseline);
  list.emplace_back(request, LayoutUnit(123));
  EXPECT_FALSE(list.IsEmpty());
  EXPECT_EQ(list.Offset(request), LayoutUnit(123));
  EXPECT_FALSE(list.Offset({NGBaselineAlgorithmType::kFirstLine,
                            FontBaseline::kIdeographicBaseline}));
}

}  // namespace blink
