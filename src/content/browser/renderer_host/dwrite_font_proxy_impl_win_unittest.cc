// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/dwrite_font_proxy_impl_win.h"

#include <dwrite.h>
#include <dwrite_2.h>

#include <memory>

#include "base/files/file.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/win/windows_version.h"
#include "content/public/common/content_features.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/service_manager/public/cpp/bind_source_info.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/dwrite_rasterizer_support/dwrite_rasterizer_support.h"
#include "third_party/blink/public/common/font_unique_name_lookup/font_table_matcher.h"

namespace content {

namespace {

std::vector<std::pair<std::string, uint32_t>> expected_test_fonts = {
    {u8"CambriaMath", 1},
    {u8"Ming-Lt-HKSCS-ExtB", 2},
    {u8"NSimSun", 1},
    {u8"calibri-bolditalic", 0}};

class DWriteFontProxyImplUnitTest : public testing::Test {
 public:
  DWriteFontProxyImplUnitTest()
      : binding_(&impl_, mojo::MakeRequest(&dwrite_font_proxy_)) {}

  blink::mojom::DWriteFontProxy& dwrite_font_proxy() {
    return *dwrite_font_proxy_;
  }

  base::test::ScopedTaskEnvironment scoped_task_environment_;
  blink::mojom::DWriteFontProxyPtr dwrite_font_proxy_;
  DWriteFontProxyImpl impl_;
  mojo::Binding<blink::mojom::DWriteFontProxy> binding_;
};

class DWriteFontProxyUniqueNameMatchingTest
    : public DWriteFontProxyImplUnitTest {
 public:
  DWriteFontProxyUniqueNameMatchingTest() {
    feature_list_.InitAndEnableFeature(features::kFontSrcLocalMatching);
    DWriteFontLookupTableBuilder::GetInstance()->ResetLookupTableForTesting();
    DWriteFontLookupTableBuilder::GetInstance()
        ->ScheduleBuildFontUniqueNameTable();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(DWriteFontProxyImplUnitTest, GetFamilyCount) {
  UINT32 family_count = 0;
  dwrite_font_proxy().GetFamilyCount(&family_count);
  EXPECT_NE(0u, family_count);  // Assume there's some fonts on the test system.
}

TEST_F(DWriteFontProxyImplUnitTest, FindFamily) {
  UINT32 arial_index = 0;
  dwrite_font_proxy().FindFamily(L"Arial", &arial_index);
  EXPECT_NE(UINT_MAX, arial_index);

  UINT32 times_index = 0;
  dwrite_font_proxy().FindFamily(L"Times New Roman", &times_index);
  EXPECT_NE(UINT_MAX, times_index);
  EXPECT_NE(arial_index, times_index);

  UINT32 unknown_index = 0;
  dwrite_font_proxy().FindFamily(L"Not a font family", &unknown_index);
  EXPECT_EQ(UINT_MAX, unknown_index);
}

TEST_F(DWriteFontProxyImplUnitTest, GetFamilyNames) {
  UINT32 arial_index = 0;
  dwrite_font_proxy().FindFamily(L"Arial", &arial_index);

  std::vector<blink::mojom::DWriteStringPairPtr> names;
  dwrite_font_proxy().GetFamilyNames(arial_index, &names);

  EXPECT_LT(0u, names.size());
  for (const auto& pair : names) {
    EXPECT_FALSE(pair->first.empty());
    EXPECT_FALSE(pair->second.empty());
  }
}

TEST_F(DWriteFontProxyImplUnitTest, GetFamilyNamesIndexOutOfBounds) {
  std::vector<blink::mojom::DWriteStringPairPtr> names;
  UINT32 invalid_index = 1000000;
  dwrite_font_proxy().GetFamilyNames(invalid_index, &names);

  EXPECT_TRUE(names.empty());
}

TEST_F(DWriteFontProxyImplUnitTest, GetFontFiles) {
  UINT32 arial_index = 0;
  dwrite_font_proxy().FindFamily(L"Arial", &arial_index);

  std::vector<base::FilePath> files;
  std::vector<base::File> handles;
  dwrite_font_proxy().GetFontFiles(arial_index, &files, &handles);

  EXPECT_LT(0u, files.size());
  for (const auto& file : files) {
    EXPECT_FALSE(file.value().empty());
  }
}

TEST_F(DWriteFontProxyImplUnitTest, GetFontFilesIndexOutOfBounds) {
  std::vector<base::FilePath> files;
  std::vector<base::File> handles;
  UINT32 invalid_index = 1000000;
  dwrite_font_proxy().GetFontFiles(invalid_index, &files, &handles);

  EXPECT_EQ(0u, files.size());
}

TEST_F(DWriteFontProxyImplUnitTest, MapCharacter) {
  if (!blink::DWriteRasterizerSupport::IsDWriteFactory2Available())
    return;

  blink::mojom::MapCharactersResultPtr result;
  dwrite_font_proxy().MapCharacters(
      L"abc",
      blink::mojom::DWriteFontStyle::New(DWRITE_FONT_WEIGHT_NORMAL,
                                         DWRITE_FONT_STYLE_NORMAL,
                                         DWRITE_FONT_STRETCH_NORMAL),
      L"", DWRITE_READING_DIRECTION_LEFT_TO_RIGHT, L"", &result);

  EXPECT_NE(UINT32_MAX, result->family_index);
  EXPECT_STRNE(L"", result->family_name.c_str());
  EXPECT_EQ(3u, result->mapped_length);
  EXPECT_NE(0.0, result->scale);
  EXPECT_NE(0, result->font_style->font_weight);
  EXPECT_EQ(DWRITE_FONT_STYLE_NORMAL, result->font_style->font_slant);
  EXPECT_NE(0, result->font_style->font_stretch);
}

TEST_F(DWriteFontProxyImplUnitTest, MapCharacterInvalidCharacter) {
  if (!blink::DWriteRasterizerSupport::IsDWriteFactory2Available())
    return;

  blink::mojom::MapCharactersResultPtr result;
  dwrite_font_proxy().MapCharacters(
      L"\ufffe\uffffabc",
      blink::mojom::DWriteFontStyle::New(DWRITE_FONT_WEIGHT_NORMAL,
                                         DWRITE_FONT_STYLE_NORMAL,
                                         DWRITE_FONT_STRETCH_NORMAL),
      L"en-us", DWRITE_READING_DIRECTION_LEFT_TO_RIGHT, L"", &result);

  EXPECT_EQ(UINT32_MAX, result->family_index);
  EXPECT_STREQ(L"", result->family_name.c_str());
  EXPECT_EQ(2u, result->mapped_length);
}

TEST_F(DWriteFontProxyImplUnitTest, MapCharacterInvalidAfterValid) {
  if (!blink::DWriteRasterizerSupport::IsDWriteFactory2Available())
    return;

  blink::mojom::MapCharactersResultPtr result;
  dwrite_font_proxy().MapCharacters(
      L"abc\ufffe\uffff",
      blink::mojom::DWriteFontStyle::New(DWRITE_FONT_WEIGHT_NORMAL,
                                         DWRITE_FONT_STYLE_NORMAL,
                                         DWRITE_FONT_STRETCH_NORMAL),
      L"en-us", DWRITE_READING_DIRECTION_LEFT_TO_RIGHT, L"", &result);

  EXPECT_NE(UINT32_MAX, result->family_index);
  EXPECT_STRNE(L"", result->family_name.c_str());
  EXPECT_EQ(3u, result->mapped_length);
  EXPECT_NE(0.0, result->scale);
  EXPECT_NE(0, result->font_style->font_weight);
  EXPECT_EQ(DWRITE_FONT_STYLE_NORMAL, result->font_style->font_slant);
  EXPECT_NE(0, result->font_style->font_stretch);
}

TEST_F(DWriteFontProxyImplUnitTest, TestCustomFontFiles) {
  // Override windows fonts path to force the custom font file codepath.
  impl_.SetWindowsFontsPathForTesting(L"X:\\NotWindowsFonts");

  UINT32 arial_index = 0;
  dwrite_font_proxy().FindFamily(L"Arial", &arial_index);

  std::vector<base::FilePath> files;
  std::vector<base::File> handles;
  dwrite_font_proxy().GetFontFiles(arial_index, &files, &handles);

  EXPECT_TRUE(files.empty());
  EXPECT_FALSE(handles.empty());
  for (auto& file : handles) {
    EXPECT_TRUE(file.IsValid());
    EXPECT_LT(0, file.GetLength());  // Check the file exists
  }
}

TEST_F(DWriteFontProxyUniqueNameMatchingTest, TestFindUniqueFont) {
  base::ReadOnlySharedMemoryRegion font_table_memory;
  dwrite_font_proxy().GetUniqueNameLookupTable(&font_table_memory);
  blink::FontTableMatcher font_table_matcher(font_table_memory.Map());

  for (auto& test_font_name_index : expected_test_fonts) {
    base::Optional<blink::FontTableMatcher::MatchResult> match_result =
        font_table_matcher.MatchName(test_font_name_index.first);
    CHECK(match_result) << "No font matched for font name: "
                        << test_font_name_index.first;
    base::File unique_font_file(
        base::FilePath::FromUTF8Unsafe(match_result->font_path),
        base::File::FLAG_OPEN | base::File::FLAG_READ);
    CHECK(unique_font_file.IsValid());
    CHECK_GT(unique_font_file.GetLength(), 0);
    CHECK_EQ(test_font_name_index.second, match_result->ttc_index);
  }
}

}  // namespace

}  // namespace content
