// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/resolver/MatchResult.h"

#include "core/css/StylePropertySet.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class MatchResultTest : public ::testing::Test {
 protected:
  void SetUp() override;

  const StylePropertySet* PropertySet(unsigned index) const {
    return property_sets[index].Get();
  }

 private:
  PersistentHeapVector<Member<MutableStylePropertySet>, 6> property_sets;
};

void MatchResultTest::SetUp() {
  for (unsigned i = 0; i < 6; i++)
    property_sets.push_back(MutableStylePropertySet::Create(kHTMLQuirksMode));
}

void TestMatchedPropertiesRange(const MatchedPropertiesRange& range,
                                int expected_length,
                                const StylePropertySet** expected_sets) {
  EXPECT_EQ(expected_length, range.end() - range.begin());
  for (const auto& matched_properties : range)
    EXPECT_EQ(*expected_sets++, matched_properties.properties);
}

TEST_F(MatchResultTest, UARules) {
  const StylePropertySet* ua_sets[] = {PropertySet(0), PropertySet(1)};

  MatchResult result;
  result.AddMatchedProperties(ua_sets[0]);
  result.AddMatchedProperties(ua_sets[1]);
  result.FinishAddingUARules();

  result.FinishAddingAuthorRulesForTreeScope();

  TestMatchedPropertiesRange(result.AllRules(), 2, ua_sets);
  TestMatchedPropertiesRange(result.UaRules(), 2, ua_sets);
  TestMatchedPropertiesRange(result.AuthorRules(), 0, nullptr);

  ImportantAuthorRanges important(result);
  EXPECT_EQ(important.end(), important.begin());
}

TEST_F(MatchResultTest, AuthorRules) {
  const StylePropertySet* author_sets[] = {PropertySet(0), PropertySet(1)};

  MatchResult result;

  result.FinishAddingUARules();
  result.AddMatchedProperties(author_sets[0]);
  result.AddMatchedProperties(author_sets[1]);
  result.FinishAddingAuthorRulesForTreeScope();

  TestMatchedPropertiesRange(result.AllRules(), 2, author_sets);
  TestMatchedPropertiesRange(result.UaRules(), 0, nullptr);
  TestMatchedPropertiesRange(result.AuthorRules(), 2, author_sets);

  ImportantAuthorRanges important(result);
  EXPECT_EQ(important.end(), ++important.begin());
}

TEST_F(MatchResultTest, UAAndAuthorRules) {
  const StylePropertySet* all_sets[] = {PropertySet(0), PropertySet(1),
                                        PropertySet(2), PropertySet(3)};
  const StylePropertySet** ua_sets = &all_sets[0];
  const StylePropertySet** author_sets = &all_sets[2];

  MatchResult result;

  result.AddMatchedProperties(ua_sets[0]);
  result.AddMatchedProperties(ua_sets[1]);
  result.FinishAddingUARules();

  result.AddMatchedProperties(author_sets[0]);
  result.AddMatchedProperties(author_sets[1]);
  result.FinishAddingAuthorRulesForTreeScope();

  TestMatchedPropertiesRange(result.AllRules(), 4, all_sets);
  TestMatchedPropertiesRange(result.UaRules(), 2, ua_sets);
  TestMatchedPropertiesRange(result.AuthorRules(), 2, author_sets);

  ImportantAuthorRanges important(result);
  EXPECT_EQ(important.end(), ++important.begin());
}

TEST_F(MatchResultTest, AuthorRulesMultipleScopes) {
  const StylePropertySet* author_sets[] = {PropertySet(0), PropertySet(1),
                                           PropertySet(2), PropertySet(3)};

  MatchResult result;

  result.FinishAddingUARules();

  result.AddMatchedProperties(author_sets[0]);
  result.AddMatchedProperties(author_sets[1]);
  result.FinishAddingAuthorRulesForTreeScope();

  result.AddMatchedProperties(author_sets[2]);
  result.AddMatchedProperties(author_sets[3]);
  result.FinishAddingAuthorRulesForTreeScope();

  TestMatchedPropertiesRange(result.AllRules(), 4, author_sets);
  TestMatchedPropertiesRange(result.UaRules(), 0, nullptr);
  TestMatchedPropertiesRange(result.AuthorRules(), 4, author_sets);

  ImportantAuthorRanges important(result);

  auto iter = important.begin();
  EXPECT_NE(important.end(), iter);
  TestMatchedPropertiesRange(*iter, 2, &author_sets[2]);

  ++iter;
  EXPECT_NE(important.end(), iter);
  TestMatchedPropertiesRange(*iter, 2, author_sets);

  ++iter;
  EXPECT_EQ(important.end(), iter);
}

TEST_F(MatchResultTest, UARulesAndAuthorRulesMultipleScopes) {
  const StylePropertySet* all_sets[] = {PropertySet(0), PropertySet(1),
                                        PropertySet(2), PropertySet(3),
                                        PropertySet(4), PropertySet(5)};
  const StylePropertySet** ua_sets = &all_sets[0];
  const StylePropertySet** author_sets = &all_sets[2];

  MatchResult result;

  result.AddMatchedProperties(ua_sets[0]);
  result.AddMatchedProperties(ua_sets[1]);
  result.FinishAddingUARules();

  result.AddMatchedProperties(author_sets[0]);
  result.AddMatchedProperties(author_sets[1]);
  result.FinishAddingAuthorRulesForTreeScope();

  result.AddMatchedProperties(author_sets[2]);
  result.AddMatchedProperties(author_sets[3]);
  result.FinishAddingAuthorRulesForTreeScope();

  TestMatchedPropertiesRange(result.AllRules(), 6, all_sets);
  TestMatchedPropertiesRange(result.UaRules(), 2, ua_sets);
  TestMatchedPropertiesRange(result.AuthorRules(), 4, author_sets);

  ImportantAuthorRanges important(result);

  ImportantAuthorRangeIterator iter = important.begin();
  EXPECT_NE(important.end(), iter);
  TestMatchedPropertiesRange(*iter, 2, &author_sets[2]);

  ++iter;
  EXPECT_NE(important.end(), iter);
  TestMatchedPropertiesRange(*iter, 2, author_sets);

  ++iter;
  EXPECT_EQ(important.end(), iter);
}

}  // namespace blink
