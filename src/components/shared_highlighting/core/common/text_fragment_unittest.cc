// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/shared_highlighting/core/common/text_fragment.h"

#include "base/values.h"
#include "components/shared_highlighting/core/common/fragment_directives_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace shared_highlighting {
namespace {

base::Value TextFragmentToValue(const std::string& fragment) {
  absl::optional<TextFragment> opt_frag =
      TextFragment::FromEscapedString(fragment);
  return opt_frag ? opt_frag->ToValue() : base::Value(base::Value::Type::NONE);
}

TEST(TextFragmentTest, FragmentToValueFromEncodedString) {
  // Success cases
  std::string fragment = "start";
  base::Value result = TextFragmentToValue(fragment);
  EXPECT_FALSE(result.FindKey(kFragmentPrefixKey));
  EXPECT_EQ("start", result.FindKey(kFragmentTextStartKey)->GetString());
  EXPECT_FALSE(result.FindKey(kFragmentTextEndKey));
  EXPECT_FALSE(result.FindKey(kFragmentSuffixKey));

  fragment = "start,end";
  result = TextFragmentToValue(fragment);
  EXPECT_FALSE(result.FindKey(kFragmentPrefixKey));
  EXPECT_EQ("start", result.FindKey(kFragmentTextStartKey)->GetString());
  EXPECT_EQ("end", result.FindKey(kFragmentTextEndKey)->GetString());
  EXPECT_FALSE(result.FindKey(kFragmentSuffixKey));

  fragment = "prefix-,start";
  result = TextFragmentToValue(fragment);
  EXPECT_EQ("prefix", result.FindKey(kFragmentPrefixKey)->GetString());
  EXPECT_EQ("start", result.FindKey(kFragmentTextStartKey)->GetString());
  EXPECT_FALSE(result.FindKey(kFragmentTextEndKey));
  EXPECT_FALSE(result.FindKey(kFragmentSuffixKey));

  fragment = "start,-suffix";
  result = TextFragmentToValue(fragment);
  EXPECT_FALSE(result.FindKey(kFragmentPrefixKey));
  EXPECT_EQ("start", result.FindKey(kFragmentTextStartKey)->GetString());
  EXPECT_FALSE(result.FindKey(kFragmentTextEndKey));
  EXPECT_EQ("suffix", result.FindKey(kFragmentSuffixKey)->GetString());

  fragment = "prefix-,start,end";
  result = TextFragmentToValue(fragment);
  EXPECT_EQ("prefix", result.FindKey(kFragmentPrefixKey)->GetString());
  EXPECT_EQ("start", result.FindKey(kFragmentTextStartKey)->GetString());
  EXPECT_EQ("end", result.FindKey(kFragmentTextEndKey)->GetString());
  EXPECT_FALSE(result.FindKey(kFragmentSuffixKey));

  fragment = "start,end,-suffix";
  result = TextFragmentToValue(fragment);
  EXPECT_FALSE(result.FindKey(kFragmentPrefixKey));
  EXPECT_EQ("start", result.FindKey(kFragmentTextStartKey)->GetString());
  EXPECT_EQ("end", result.FindKey(kFragmentTextEndKey)->GetString());
  EXPECT_EQ("suffix", result.FindKey(kFragmentSuffixKey)->GetString());

  fragment = "prefix-,start,end,-suffix";
  result = TextFragmentToValue(fragment);
  EXPECT_EQ("prefix", result.FindKey(kFragmentPrefixKey)->GetString());
  EXPECT_EQ("start", result.FindKey(kFragmentTextStartKey)->GetString());
  EXPECT_EQ("end", result.FindKey(kFragmentTextEndKey)->GetString());
  EXPECT_EQ("suffix", result.FindKey(kFragmentSuffixKey)->GetString());

  // Trailing comma doesn't break otherwise valid fragment
  fragment = "start,";
  result = TextFragmentToValue(fragment);
  EXPECT_FALSE(result.FindKey(kFragmentPrefixKey));
  EXPECT_EQ("start", result.FindKey(kFragmentTextStartKey)->GetString());
  EXPECT_FALSE(result.FindKey(kFragmentTextEndKey));
  EXPECT_FALSE(result.FindKey(kFragmentSuffixKey));

  // Failure Cases
  fragment = "";
  result = TextFragmentToValue(fragment);
  EXPECT_EQ(base::Value::Type::NONE, result.type());

  fragment = "some,really-,malformed,-thing,with,too,many,commas";
  result = TextFragmentToValue(fragment);
  EXPECT_EQ(base::Value::Type::NONE, result.type());

  fragment = "prefix-,-suffix";
  result = TextFragmentToValue(fragment);
  EXPECT_EQ(base::Value::Type::NONE, result.type());

  fragment = "start,prefix-,-suffix";
  result = TextFragmentToValue(fragment);
  EXPECT_EQ(base::Value::Type::NONE, result.type());

  fragment = "prefix-,-suffix,start";
  result = TextFragmentToValue(fragment);
  EXPECT_EQ(base::Value::Type::NONE, result.type());

  fragment = "prefix-";
  result = TextFragmentToValue(fragment);
  EXPECT_EQ(base::Value::Type::NONE, result.type());

  fragment = "-suffix";
  result = TextFragmentToValue(fragment);
  EXPECT_EQ(base::Value::Type::NONE, result.type());
}

TEST(TextFragmentTest, FragmentToEscapedStringEmpty) {
  EXPECT_EQ("", TextFragment("").ToEscapedString());
}

TEST(TextFragmentTest, FragmentToEscapedStringEmptyTextStart) {
  EXPECT_EQ("", TextFragment("", "a", "b", "c").ToEscapedString());
}

TEST(TextFragmentTest, FragmentToEscapedStringOnlyTextStart) {
  EXPECT_EQ("text=only%20start", TextFragment("only start").ToEscapedString());
}

TEST(TextFragmentTest, FragmentToEscapedStringWithTextEnd) {
  EXPECT_EQ("text=only%20start,and%20end",
            TextFragment("only start", "and end", "", "").ToEscapedString());
}

TEST(TextFragmentTest, FragmentToEscapedStringWithPrefix) {
  EXPECT_EQ("text=and%20prefix-,only%20start",
            TextFragment("only start", "", "and prefix", "").ToEscapedString());
}

TEST(TextFragmentTest, FragmentToEscapedStringWithPrefixAndSuffix) {
  EXPECT_EQ("text=and%20prefix-,only%20start,-and%20suffix",
            TextFragment("only start", "", "and prefix", "and suffix")
                .ToEscapedString());
}

TEST(TextFragmentTest, FragmentToEscapedStringAllWithSpecialCharacters) {
  TextFragment test_fragment("text, Start-&", "end of, & Text-", "pre-fix&, !",
                             "suff,i,x-+&");
  EXPECT_EQ(
      "text=pre%2Dfix%26%2C%20!-,"
      "text%2C%20Start%2D%26"
      ",end%20of%2C%20%26%20Text%2D"
      ",-suff%2Ci%2Cx%2D%2B%26",
      test_fragment.ToEscapedString());
}

TEST(TextFragmentTest, FromValue) {
  const char text_start[] = "test text start, * - &";
  const char text_end[] = "test text end, * - &";
  const char prefix[] = "prefix, * - &";
  const char suffix[] = "suffix, * - &";

  base::Value fragment_value = base::Value(base::Value::Type::DICTIONARY);

  // Empty value cases.
  EXPECT_FALSE(TextFragment::FromValue(&fragment_value).has_value());
  EXPECT_FALSE(TextFragment::FromValue(nullptr).has_value());
  base::Value string_value = base::Value(base::Value::Type::STRING);
  EXPECT_FALSE(TextFragment::FromValue(&string_value).has_value());

  fragment_value.SetStringKey(kFragmentTextStartKey, text_start);
  fragment_value.SetStringKey(kFragmentTextEndKey, text_end);
  fragment_value.SetStringKey(kFragmentPrefixKey, prefix);
  fragment_value.SetStringKey(kFragmentSuffixKey, suffix);

  absl::optional<TextFragment> opt_fragment =
      TextFragment::FromValue(&fragment_value);
  EXPECT_TRUE(opt_fragment.has_value());
  TextFragment fragment = opt_fragment.value();
  EXPECT_EQ(text_start, fragment.text_start());
  EXPECT_EQ(text_end, fragment.text_end());
  EXPECT_EQ(prefix, fragment.prefix());
  EXPECT_EQ(suffix, fragment.suffix());

  // Testing the case where the dictionary value doesn't have a text start
  // value.
  ASSERT_TRUE(fragment_value.RemoveKey(kFragmentTextStartKey));
  EXPECT_FALSE(TextFragment::FromValue(&fragment_value).has_value());
}

}  // namespace
}  // namespace shared_highlighting
