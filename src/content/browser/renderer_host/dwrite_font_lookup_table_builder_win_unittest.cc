// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/dwrite_font_lookup_table_builder_win.h"

#include <string>
#include <utility>
#include <vector>

#include "base/files/file.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "content/public/common/content_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/font_unique_name_lookup/font_table_matcher.h"

namespace content {

namespace {

std::vector<std::pair<std::string, uint32_t>> expected_test_fonts = {
    {u8"CambriaMath", 1},
    {u8"Ming-Lt-HKSCS-ExtB", 2},
    {u8"NSimSun", 1},
    {u8"calibri-bolditalic", 0}};

class DWriteFontLookupTableBuilderTest : public testing::Test {
 public:
  DWriteFontLookupTableBuilderTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::ExecutionMode::ASYNC) {
    feature_list_.InitAndEnableFeature(features::kFontSrcLocalMatching);
  }

  void SetUp() override {
    font_lookup_table_builder_ = DWriteFontLookupTableBuilder::GetInstance();
    font_lookup_table_builder_->ResetLookupTableForTesting();
  }

 protected:
  DWriteFontLookupTableBuilder* font_lookup_table_builder_;

 private:
  base::test::ScopedFeatureList feature_list_;
  base::test::ScopedTaskEnvironment scoped_task_environment_;
};

class DWriteFontLookupTableBuilderTimeoutTest
    : public DWriteFontLookupTableBuilderTest,
      public ::testing::WithParamInterface<
          DWriteFontLookupTableBuilder::SlowDownMode> {};

}  // namespace

// Run a test similar to DWriteFontProxyImplUnitTest, TestFindUniqueFont but
// without going through Mojo and running it on the DWRiteFontLookupTableBuilder
// class directly.
TEST_F(DWriteFontLookupTableBuilderTest, TestFindUniqueFontDirect) {
  font_lookup_table_builder_->ScheduleBuildFontUniqueNameTable();
  font_lookup_table_builder_->EnsureFontUniqueNameTable();
  base::ReadOnlySharedMemoryRegion font_table_memory =
      font_lookup_table_builder_->DuplicateMemoryRegion();
  blink::FontTableMatcher font_table_matcher(font_table_memory.Map());

  for (auto& test_font_name_index : expected_test_fonts) {
    base::Optional<blink::FontTableMatcher::MatchResult> match_result =
        font_table_matcher.MatchName(test_font_name_index.first);
    ASSERT_TRUE(match_result)
        << "No font matched for font name: " << test_font_name_index.first;
    base::File unique_font_file(
        base::FilePath::FromUTF8Unsafe(match_result->font_path),
        base::File::FLAG_OPEN | base::File::FLAG_READ);
    ASSERT_TRUE(unique_font_file.IsValid());
    ASSERT_GT(unique_font_file.GetLength(), 0);
    ASSERT_EQ(test_font_name_index.second, match_result->ttc_index);
  }
}

TEST_P(DWriteFontLookupTableBuilderTimeoutTest, TestTimeout) {
  font_lookup_table_builder_->SetSlowDownIndexingForTesting(GetParam());
  font_lookup_table_builder_->ScheduleBuildFontUniqueNameTable();
  font_lookup_table_builder_->EnsureFontUniqueNameTable();
  base::ReadOnlySharedMemoryRegion font_table_memory =
      font_lookup_table_builder_->DuplicateMemoryRegion();
  blink::FontTableMatcher font_table_matcher(font_table_memory.Map());

  for (auto& test_font_name_index : expected_test_fonts) {
    base::Optional<blink::FontTableMatcher::MatchResult> match_result =
        font_table_matcher.MatchName(test_font_name_index.first);
    ASSERT_TRUE(!match_result);
  }
  if (GetParam() == DWriteFontLookupTableBuilder::SlowDownMode::kHangOneTask)
    font_lookup_table_builder_->ResumeFromHangForTesting();
}

INSTANTIATE_TEST_SUITE_P(
    ,
    DWriteFontLookupTableBuilderTimeoutTest,
    ::testing::Values(
        DWriteFontLookupTableBuilder::SlowDownMode::kDelayEachTask,
        DWriteFontLookupTableBuilder::SlowDownMode::kHangOneTask));

TEST_F(DWriteFontLookupTableBuilderTest, RepeatedScheduling) {
  for (unsigned i = 0; i < 3; ++i) {
    font_lookup_table_builder_->ResetLookupTableForTesting();
    font_lookup_table_builder_->ScheduleBuildFontUniqueNameTable();
    font_lookup_table_builder_->EnsureFontUniqueNameTable();
  }
}

}  // namespace content
