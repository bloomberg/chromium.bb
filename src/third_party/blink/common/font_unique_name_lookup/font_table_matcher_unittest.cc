// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/font_unique_name_lookup/font_table_matcher.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/font_unique_name_lookup/icu_fold_case_util.h"

namespace {
const char kTestFilePath1[] = "tmp/test/font1.ttf";
const char kDummyAndroidBuildFingerPrint[] = "A";

void PopulateFontUniqueNameEntry(
    blink::FontUniqueNameTable_FontUniqueNameEntry* entry,
    const std::string& path,
    int32_t ttc_index,
    const std::string& full_name,
    const std::string& postscript_name) {
  entry->set_file_path(path);
  entry->set_ttc_index(ttc_index);
  entry->set_full_name(blink::IcuFoldCase(full_name));
  entry->set_postscript_name(blink::IcuFoldCase(postscript_name));
}

}  // namespace

namespace blink {

class FontTableMatcherTest : public ::testing::Test {
 protected:
  void SetUp() override {
    FontUniqueNameTable font_unique_name_table;
    font_unique_name_table.set_stored_for_android_build_fp(
        kDummyAndroidBuildFingerPrint);
    PopulateFontUniqueNameEntry(font_unique_name_table.add_font_entries(),
                                kTestFilePath1, 0, "FONT NAME UPPERCASE",
                                "FONT-NAME-UPPERCASE");
    base::ReadOnlySharedMemoryMapping mapping =
        FontTableMatcher::MemoryMappingFromFontUniqueNameTable(
            std::move(font_unique_name_table));

    matcher_ = std::make_unique<FontTableMatcher>(mapping);
  }

  std::unique_ptr<FontTableMatcher> matcher_;
};

TEST_F(FontTableMatcherTest, CaseInsensitiveMatchingBothNames) {
  ASSERT_EQ(matcher_->AvailableFonts(), 1u);
  base::Optional<FontTableMatcher::MatchResult> result =
      matcher_->MatchName("font name uppercase");
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result->font_path, kTestFilePath1);
  ASSERT_EQ(result->ttc_index, 0u);

  result = matcher_->MatchName("font-name-uppercase");
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result->font_path, kTestFilePath1);
  ASSERT_EQ(result->ttc_index, 0u);
}

TEST_F(FontTableMatcherTest, NoSubStringMatching) {
  ASSERT_EQ(matcher_->AvailableFonts(), 1u);
  base::Optional<FontTableMatcher::MatchResult> result =
      matcher_->MatchName("font name");
  ASSERT_FALSE(result.has_value());

  result = matcher_->MatchName("font-name");
  ASSERT_FALSE(result.has_value());
}

}  // namespace blink
