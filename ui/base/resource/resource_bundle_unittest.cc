// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/resource/resource_bundle.h"

#include "base/base_paths.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/path_service.h"
#include "base/scoped_temp_dir.h"
#include "base/utf_string_conversions.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/layout.h"

using ::testing::_;
using ::testing::Between;
using ::testing::Property;
using ::testing::Return;
using ::testing::ReturnArg;

namespace ui {

extern const char kSamplePakContents[];
extern const size_t kSamplePakSize;
extern const char kSamplePakContents2x[];
extern const size_t kSamplePakSize2x;
extern const char kEmptyPakContents[];
extern const size_t kEmptyPakSize;

namespace {

// Mock for the ResourceBundle::Delegate class.
class MockResourceBundleDelegate : public ui::ResourceBundle::Delegate {
 public:
  MockResourceBundleDelegate() {
  }
  virtual ~MockResourceBundleDelegate() {
  }

  MOCK_METHOD2(GetPathForResourcePack, FilePath(const FilePath& pack_path,
                                                ui::ScaleFactor scale_factor));
  MOCK_METHOD2(GetPathForLocalePack, FilePath(const FilePath& pack_path,
                                              const std::string& locale));
  MOCK_METHOD1(GetImageNamed, gfx::Image(int resource_id));
  MOCK_METHOD2(GetNativeImageNamed,
      gfx::Image(int resource_id,
                 ui::ResourceBundle::ImageRTL rtl));
  MOCK_METHOD2(LoadDataResourceBytes,
      base::RefCountedStaticMemory*(int resource_id,
                                    ui::ScaleFactor scale_factor));
  MOCK_METHOD2(GetRawDataResourceMock, base::StringPiece(
      int resource_id,
      ui::ScaleFactor scale_factor));
  virtual bool GetRawDataResource(int resource_id,
                                  ui::ScaleFactor scale_factor,
                                  base::StringPiece* value) OVERRIDE {
    *value = GetRawDataResourceMock(resource_id, scale_factor);
    return true;
  }
  MOCK_METHOD1(GetLocalizedStringMock, string16(int message_id));
  virtual bool GetLocalizedString(int message_id, string16* value) OVERRIDE {
    *value = GetLocalizedStringMock(message_id);
    return true;
  }
  MOCK_METHOD1(GetFontMock, gfx::Font*(ui::ResourceBundle::FontStyle style));
  virtual scoped_ptr<gfx::Font> GetFont(
      ui::ResourceBundle::FontStyle style) OVERRIDE {
    return scoped_ptr<gfx::Font>(GetFontMock(style));
  }
};

}  // namespace

TEST(ResourceBundle, DelegateGetPathForResourcePack) {
  MockResourceBundleDelegate delegate;
  ResourceBundle resource_bundle(&delegate);

  FilePath pack_path(FILE_PATH_LITERAL("/path/to/test_path.pak"));
  ui::ScaleFactor pack_scale_factor = ui::SCALE_FACTOR_200P;

  EXPECT_CALL(delegate,
      GetPathForResourcePack(
          Property(&FilePath::value, pack_path.value()),
          pack_scale_factor))
      .Times(1)
      .WillOnce(Return(pack_path));

  resource_bundle.AddDataPack(pack_path, pack_scale_factor);
}

TEST(ResourceBundle, DelegateGetPathForLocalePack) {
  MockResourceBundleDelegate delegate;
  ResourceBundle resource_bundle(&delegate);

  std::string locale = "en-US";

  // Cancel the load.
  EXPECT_CALL(delegate, GetPathForLocalePack(_, locale))
      .Times(2)
      .WillRepeatedly(Return(FilePath()))
      .RetiresOnSaturation();

  EXPECT_FALSE(resource_bundle.LocaleDataPakExists(locale));
  EXPECT_EQ("", resource_bundle.LoadLocaleResources(locale));

  // Allow the load to proceed.
  EXPECT_CALL(delegate, GetPathForLocalePack(_, locale))
      .Times(2)
      .WillRepeatedly(ReturnArg<0>());

  EXPECT_TRUE(resource_bundle.LocaleDataPakExists(locale));
  EXPECT_EQ(locale, resource_bundle.LoadLocaleResources(locale));
}

TEST(ResourceBundle, DelegateGetImageNamed) {
  MockResourceBundleDelegate delegate;
  ResourceBundle resource_bundle(&delegate);

  gfx::Image empty_image = resource_bundle.GetEmptyImage();
  int resource_id = 5;

  EXPECT_CALL(delegate, GetImageNamed(resource_id))
      .Times(1)
      .WillOnce(Return(empty_image));

  gfx::Image result = resource_bundle.GetImageNamed(resource_id);
  EXPECT_EQ(empty_image.ToSkBitmap(), result.ToSkBitmap());
}

TEST(ResourceBundle, DelegateGetNativeImageNamed) {
  MockResourceBundleDelegate delegate;
  ResourceBundle resource_bundle(&delegate);

  gfx::Image empty_image = resource_bundle.GetEmptyImage();
  int resource_id = 5;

  // Some platforms delegate GetNativeImageNamed calls to GetImageNamed.
  EXPECT_CALL(delegate, GetImageNamed(resource_id))
      .Times(Between(0, 1))
      .WillOnce(Return(empty_image));
  EXPECT_CALL(delegate,
      GetNativeImageNamed(resource_id, ui::ResourceBundle::RTL_DISABLED))
      .Times(Between(0, 1))
      .WillOnce(Return(empty_image));

  gfx::Image result = resource_bundle.GetNativeImageNamed(resource_id);
  EXPECT_EQ(empty_image.ToSkBitmap(), result.ToSkBitmap());
}

TEST(ResourceBundle, DelegateLoadDataResourceBytes) {
  MockResourceBundleDelegate delegate;
  ResourceBundle resource_bundle(&delegate);

  // Create the data resource for testing purposes.
  unsigned char data[] = "My test data";
  scoped_refptr<base::RefCountedStaticMemory> static_memory(
      new base::RefCountedStaticMemory(data, sizeof(data)));

  int resource_id = 5;
  ui::ScaleFactor scale_factor = ui::SCALE_FACTOR_NONE;

  EXPECT_CALL(delegate, LoadDataResourceBytes(resource_id, scale_factor))
      .Times(1)
      .WillOnce(Return(static_memory));

  scoped_refptr<base::RefCountedStaticMemory> result =
      resource_bundle.LoadDataResourceBytes(resource_id, scale_factor);
  EXPECT_EQ(static_memory, result);
}

TEST(ResourceBundle, DelegateGetRawDataResource) {
  MockResourceBundleDelegate delegate;
  ResourceBundle resource_bundle(&delegate);

  // Create the string piece for testing purposes.
  char data[] = "My test data";
  base::StringPiece string_piece(data);

  int resource_id = 5;

  EXPECT_CALL(delegate, GetRawDataResourceMock(
          resource_id, ui::SCALE_FACTOR_NONE))
      .Times(1)
      .WillOnce(Return(string_piece));

  base::StringPiece result = resource_bundle.GetRawDataResource(
      resource_id, ui::SCALE_FACTOR_NONE);
  EXPECT_EQ(string_piece.data(), result.data());
}

TEST(ResourceBundle, DelegateGetLocalizedString) {
  MockResourceBundleDelegate delegate;
  ResourceBundle resource_bundle(&delegate);

  string16 data = ASCIIToUTF16("My test data");
  int resource_id = 5;

  EXPECT_CALL(delegate, GetLocalizedStringMock(resource_id))
      .Times(1)
      .WillOnce(Return(data));

  string16 result = resource_bundle.GetLocalizedString(resource_id);
  EXPECT_EQ(data, result);
}

TEST(ResourceBundle, DelegateGetFont) {
  MockResourceBundleDelegate delegate;
  ResourceBundle resource_bundle(&delegate);

  // Should be called once for each font type. When we return NULL the default
  // font will be created.
  gfx::Font* test_font = NULL;
  EXPECT_CALL(delegate, GetFontMock(_))
      .Times(7)
      .WillRepeatedly(Return(test_font));

  const gfx::Font* font =
      &resource_bundle.GetFont(ui::ResourceBundle::BaseFont);
  EXPECT_TRUE(font);
}

TEST(ResourceBundle, LoadDataResourceBytes) {
  // On Windows, the default data is compiled into the binary so this does
  // nothing.
  ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  FilePath data_path = dir.path().Append(FILE_PATH_LITERAL("sample.pak"));

  // Put the ResourceBundle in a different scope so that it's destroyed before
  // the ScopedTempDir.
  {
    // Verify that we don't crash when trying to load a resource that is not
    // found.  In some cases, we fail to mmap resources.pak, but try to keep
    // going anyway.
    ResourceBundle resource_bundle(NULL);

    // Dump contents into the pak file.
    ASSERT_EQ(file_util::WriteFile(data_path, kSamplePakContents,
                                   kSamplePakSize),
              static_cast<int>(kSamplePakSize));

    // Create a resource bundle from the file.
    resource_bundle.LoadTestResources(data_path, data_path);

    const int kUnfoundResourceId = 10000;
    EXPECT_EQ(NULL, resource_bundle.LoadDataResourceBytes(
        kUnfoundResourceId, ui::SCALE_FACTOR_NONE));

    // Give a .pak file that doesn't exist so we will fail to load it.
    resource_bundle.AddDataPack(
        FilePath(FILE_PATH_LITERAL("non-existant-file.pak")),
        ui::SCALE_FACTOR_NONE);
    EXPECT_EQ(NULL, resource_bundle.LoadDataResourceBytes(
        kUnfoundResourceId, ui::SCALE_FACTOR_NONE));
  }
}

TEST(ResourceBundle, GetRawDataResource) {

  // On Windows, the default data is compiled into the binary so this does
  // nothing.
  ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  FilePath locale_path = dir.path().Append(FILE_PATH_LITERAL("empty.pak"));
  FilePath data_path = dir.path().Append(FILE_PATH_LITERAL("sample.pak"));
  FilePath data_2x_path = dir.path().Append(FILE_PATH_LITERAL("sample_2x.pak"));

  {
    ResourceBundle resource_bundle(NULL);
    // Dump contents into the pak files.
    ASSERT_EQ(file_util::WriteFile(locale_path, kEmptyPakContents,
        kEmptyPakSize), static_cast<int>(kEmptyPakSize));
    ASSERT_EQ(file_util::WriteFile(data_path, kSamplePakContents,
        kSamplePakSize), static_cast<int>(kSamplePakSize));
    ASSERT_EQ(file_util::WriteFile(data_2x_path, kSamplePakContents2x,
        kSamplePakSize2x), static_cast<int>(kSamplePakSize2x));

    // Load the regular and 2x pak files.
    resource_bundle.LoadTestResources(data_path, locale_path);
    resource_bundle.AddDataPack(data_2x_path, SCALE_FACTOR_200P);

    // Resource ID 4 exists in both 1x and 2x paks, so we expect a different
    // result when requesting the 2x scale.
    EXPECT_EQ("this is id 4", resource_bundle.GetRawDataResource(4,
        SCALE_FACTOR_100P));
    EXPECT_EQ("this is id 4 2x", resource_bundle.GetRawDataResource(4,
        SCALE_FACTOR_200P));

    // Resource ID 6 only exists in the 1x pak so we expect the same resource
    // for both scale factor requests.
    EXPECT_EQ("this is id 6", resource_bundle.GetRawDataResource(6,
        SCALE_FACTOR_100P));
    EXPECT_EQ("this is id 6", resource_bundle.GetRawDataResource(6,
        SCALE_FACTOR_200P));
  }
}

TEST(ResourceBundle, LocaleDataPakExists) {
  ResourceBundle resource_bundle(NULL);

  // Check that ResourceBundle::LocaleDataPakExists returns the correct results.
  EXPECT_TRUE(resource_bundle.LocaleDataPakExists("en-US"));
  EXPECT_FALSE(resource_bundle.LocaleDataPakExists("not_a_real_locale"));
}

}  // namespace ui
