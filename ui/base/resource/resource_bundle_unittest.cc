// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/resource/resource_bundle.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "base/base_paths.h"
#include "base/big_endian.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/layout.h"
#include "ui/base/resource/data_pack.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/image/image_skia.h"

#if defined(OS_WIN)
#include "ui/display/win/dpi.h"
#endif

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

const unsigned char kPngMagic[8] = { 0x89, 'P', 'N', 'G', 13, 10, 26, 10 };
const size_t kPngChunkMetadataSize = 12;
const unsigned char kPngIHDRChunkType[4] = { 'I', 'H', 'D', 'R' };

// Custom chunk that GRIT adds to PNG to indicate that it could not find a
// bitmap at the requested scale factor and fell back to 1x.
const unsigned char kPngScaleChunk[12] = { 0x00, 0x00, 0x00, 0x00,
                                           'c', 's', 'C', 'l',
                                           0xc1, 0x30, 0x60, 0x4d };

// Mock for the ResourceBundle::Delegate class.
class MockResourceBundleDelegate : public ui::ResourceBundle::Delegate {
 public:
  MockResourceBundleDelegate() {
  }
  ~MockResourceBundleDelegate() override {
  }

  MOCK_METHOD2(GetPathForResourcePack, base::FilePath(
      const base::FilePath& pack_path, ui::ScaleFactor scale_factor));
  MOCK_METHOD2(GetPathForLocalePack, base::FilePath(
      const base::FilePath& pack_path, const std::string& locale));
  MOCK_METHOD1(GetImageNamed, gfx::Image(int resource_id));
  MOCK_METHOD1(GetNativeImageNamed, gfx::Image(int resource_id));
  MOCK_METHOD2(LoadDataResourceBytes,
      base::RefCountedMemory*(int resource_id, ui::ScaleFactor scale_factor));
  MOCK_METHOD2(GetRawDataResourceMock, base::StringPiece(
      int resource_id,
      ui::ScaleFactor scale_factor));
  bool GetRawDataResource(int resource_id,
                          ui::ScaleFactor scale_factor,
                          base::StringPiece* value) override {
    *value = GetRawDataResourceMock(resource_id, scale_factor);
    return true;
  }
  MOCK_METHOD1(GetLocalizedStringMock, base::string16(int message_id));
  bool GetLocalizedString(int message_id,
                          base::string16* value) override {
    *value = GetLocalizedStringMock(message_id);
    return true;
  }
};

// Returns |bitmap_data| with |custom_chunk| inserted after the IHDR chunk.
void AddCustomChunk(const base::StringPiece& custom_chunk,
                    std::vector<unsigned char>* bitmap_data) {
  EXPECT_LT(arraysize(kPngMagic) + kPngChunkMetadataSize, bitmap_data->size());
  EXPECT_TRUE(std::equal(
      bitmap_data->begin(),
      bitmap_data->begin() + arraysize(kPngMagic),
      kPngMagic));
  std::vector<unsigned char>::iterator ihdr_start =
      bitmap_data->begin() + arraysize(kPngMagic);
  char ihdr_length_data[sizeof(uint32_t)];
  for (size_t i = 0; i < sizeof(uint32_t); ++i)
    ihdr_length_data[i] = *(ihdr_start + i);
  uint32_t ihdr_chunk_length = 0;
  base::ReadBigEndian(reinterpret_cast<char*>(ihdr_length_data),
                      &ihdr_chunk_length);
  EXPECT_TRUE(
      std::equal(ihdr_start + sizeof(uint32_t),
                 ihdr_start + sizeof(uint32_t) + sizeof(kPngIHDRChunkType),
                 kPngIHDRChunkType));

  bitmap_data->insert(ihdr_start + kPngChunkMetadataSize + ihdr_chunk_length,
                      custom_chunk.begin(), custom_chunk.end());
}

// Creates datapack at |path| with a single bitmap at resource ID 3
// which is |edge_size|x|edge_size| pixels.
// If |custom_chunk| is non empty, adds it after the IHDR chunk
// in the encoded bitmap data.
void CreateDataPackWithSingleBitmap(const base::FilePath& path,
                                    int edge_size,
                                    const base::StringPiece& custom_chunk) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(edge_size, edge_size);
  bitmap.eraseColor(SK_ColorWHITE);
  std::vector<unsigned char> bitmap_data;
  EXPECT_TRUE(gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, false, &bitmap_data));

  if (custom_chunk.size() > 0)
    AddCustomChunk(custom_chunk, &bitmap_data);

  std::map<uint16_t, base::StringPiece> resources;
  resources[3u] = base::StringPiece(
      reinterpret_cast<const char*>(&bitmap_data[0]), bitmap_data.size());
  DataPack::WritePack(path, resources, ui::DataPack::BINARY);
}

}  // namespace

class ResourceBundleTest : public testing::Test {
 public:
  ResourceBundleTest() : resource_bundle_(NULL) {
  }

  ~ResourceBundleTest() override {}

  // Overridden from testing::Test:
  void TearDown() override { delete resource_bundle_; }

  // Returns new ResoureBundle with the specified |delegate|. The
  // ResourceBundleTest class manages the lifetime of the returned
  // ResourceBundle.
  ResourceBundle* CreateResourceBundle(ResourceBundle::Delegate* delegate) {
    DCHECK(!resource_bundle_);

    resource_bundle_ = new ResourceBundle(delegate);
    return resource_bundle_;
  }

 protected:
  ResourceBundle* resource_bundle_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ResourceBundleTest);
};

TEST_F(ResourceBundleTest, DelegateGetPathForResourcePack) {
  MockResourceBundleDelegate delegate;
  ResourceBundle* resource_bundle = CreateResourceBundle(&delegate);

  base::FilePath pack_path(FILE_PATH_LITERAL("/path/to/test_path.pak"));
  ui::ScaleFactor pack_scale_factor = ui::SCALE_FACTOR_200P;

  EXPECT_CALL(delegate,
      GetPathForResourcePack(
          Property(&base::FilePath::value, pack_path.value()),
          pack_scale_factor))
      .Times(1)
      .WillOnce(Return(pack_path));

  resource_bundle->AddDataPackFromPath(pack_path, pack_scale_factor);
}

#if defined(OS_LINUX)
// Fails consistently on Linux: crbug.com/161902
#define MAYBE_DelegateGetPathForLocalePack DISABLED_DelegateGetPathForLocalePack
#else
#define MAYBE_DelegateGetPathForLocalePack DelegateGetPathForLocalePack
#endif
TEST_F(ResourceBundleTest, MAYBE_DelegateGetPathForLocalePack) {
  MockResourceBundleDelegate delegate;
  ResourceBundle* resource_bundle = CreateResourceBundle(&delegate);

  std::string locale = "en-US";

  // Cancel the load.
  EXPECT_CALL(delegate, GetPathForLocalePack(_, locale))
      .Times(2)
      .WillRepeatedly(Return(base::FilePath()))
      .RetiresOnSaturation();

  EXPECT_FALSE(resource_bundle->LocaleDataPakExists(locale));
  EXPECT_EQ("", resource_bundle->LoadLocaleResources(locale));

  // Allow the load to proceed.
  EXPECT_CALL(delegate, GetPathForLocalePack(_, locale))
      .Times(2)
      .WillRepeatedly(ReturnArg<0>());

  EXPECT_TRUE(resource_bundle->LocaleDataPakExists(locale));
  EXPECT_EQ(locale, resource_bundle->LoadLocaleResources(locale));
}

TEST_F(ResourceBundleTest, DelegateGetImageNamed) {
  MockResourceBundleDelegate delegate;
  ResourceBundle* resource_bundle = CreateResourceBundle(&delegate);

  gfx::Image empty_image = resource_bundle->GetEmptyImage();
  int resource_id = 5;

  EXPECT_CALL(delegate, GetImageNamed(resource_id))
      .Times(1)
      .WillOnce(Return(empty_image));

  gfx::Image result = resource_bundle->GetImageNamed(resource_id);
  EXPECT_EQ(empty_image.ToSkBitmap(), result.ToSkBitmap());
}

TEST_F(ResourceBundleTest, DelegateGetNativeImageNamed) {
  MockResourceBundleDelegate delegate;
  ResourceBundle* resource_bundle = CreateResourceBundle(&delegate);

  gfx::Image empty_image = resource_bundle->GetEmptyImage();
  int resource_id = 5;

  // Some platforms delegate GetNativeImageNamed calls to GetImageNamed.
  EXPECT_CALL(delegate, GetImageNamed(resource_id))
      .Times(Between(0, 1))
      .WillOnce(Return(empty_image));
  EXPECT_CALL(delegate,
      GetNativeImageNamed(resource_id))
      .Times(Between(0, 1))
      .WillOnce(Return(empty_image));

  gfx::Image result = resource_bundle->GetNativeImageNamed(resource_id);
  EXPECT_EQ(empty_image.ToSkBitmap(), result.ToSkBitmap());
}

TEST_F(ResourceBundleTest, DelegateLoadDataResourceBytes) {
  MockResourceBundleDelegate delegate;
  ResourceBundle* resource_bundle = CreateResourceBundle(&delegate);

  // Create the data resource for testing purposes.
  unsigned char data[] = "My test data";
  scoped_refptr<base::RefCountedStaticMemory> static_memory(
      new base::RefCountedStaticMemory(data, sizeof(data)));

  int resource_id = 5;
  ui::ScaleFactor scale_factor = ui::SCALE_FACTOR_NONE;

  EXPECT_CALL(delegate, LoadDataResourceBytes(resource_id, scale_factor))
      .Times(1).WillOnce(Return(static_memory.get()));

  scoped_refptr<base::RefCountedMemory> result =
      resource_bundle->LoadDataResourceBytesForScale(resource_id, scale_factor);
  EXPECT_EQ(static_memory, result);
}

TEST_F(ResourceBundleTest, DelegateGetRawDataResource) {
  MockResourceBundleDelegate delegate;
  ResourceBundle* resource_bundle = CreateResourceBundle(&delegate);

  // Create the string piece for testing purposes.
  char data[] = "My test data";
  base::StringPiece string_piece(data);

  int resource_id = 5;

  EXPECT_CALL(delegate, GetRawDataResourceMock(
          resource_id, ui::SCALE_FACTOR_NONE))
      .Times(1)
      .WillOnce(Return(string_piece));

  base::StringPiece result = resource_bundle->GetRawDataResource(
      resource_id);
  EXPECT_EQ(string_piece.data(), result.data());
}

TEST_F(ResourceBundleTest, DelegateGetLocalizedString) {
  MockResourceBundleDelegate delegate;
  ResourceBundle* resource_bundle = CreateResourceBundle(&delegate);

  base::string16 data = base::ASCIIToUTF16("My test data");
  int resource_id = 5;

  EXPECT_CALL(delegate, GetLocalizedStringMock(resource_id))
      .Times(1)
      .WillOnce(Return(data));

  base::string16 result = resource_bundle->GetLocalizedString(resource_id);
  EXPECT_EQ(data, result);
}

TEST_F(ResourceBundleTest, OverrideStringResource) {
  ResourceBundle* resource_bundle = CreateResourceBundle(NULL);

  base::string16 data = base::ASCIIToUTF16("My test data");
  int resource_id = 5;

  base::string16 result = resource_bundle->GetLocalizedString(resource_id);
  EXPECT_EQ(base::string16(), result);

  resource_bundle->OverrideLocaleStringResource(resource_id, data);

  result = resource_bundle->GetLocalizedString(resource_id);
  EXPECT_EQ(data, result);
}

TEST_F(ResourceBundleTest, DelegateGetLocalizedStringWithOverride) {
  MockResourceBundleDelegate delegate;
  ResourceBundle* resource_bundle = CreateResourceBundle(&delegate);

  base::string16 delegate_data = base::ASCIIToUTF16("My delegate data");
  int resource_id = 5;

  EXPECT_CALL(delegate, GetLocalizedStringMock(resource_id)).Times(1).WillOnce(
      Return(delegate_data));

  base::string16 override_data = base::ASCIIToUTF16("My override data");

  base::string16 result = resource_bundle->GetLocalizedString(resource_id);
  EXPECT_EQ(delegate_data, result);
}

TEST_F(ResourceBundleTest, LocaleDataPakExists) {
  ResourceBundle* resource_bundle = CreateResourceBundle(NULL);

  // Check that ResourceBundle::LocaleDataPakExists returns the correct results.
  EXPECT_TRUE(resource_bundle->LocaleDataPakExists("en-US"));
  EXPECT_FALSE(resource_bundle->LocaleDataPakExists("not_a_real_locale"));
}

class ResourceBundleImageTest : public ResourceBundleTest {
 public:
  ResourceBundleImageTest() {}

  ~ResourceBundleImageTest() override {}

  void SetUp() override {
    // Create a temporary directory to write test resource bundles to.
    ASSERT_TRUE(dir_.CreateUniqueTempDir());
  }

  // Returns resource bundle which uses an empty data pak for locale data.
  ui::ResourceBundle* CreateResourceBundleWithEmptyLocalePak() {
    // Write an empty data pak for locale data.
    const base::FilePath& locale_path = dir_path().Append(
        FILE_PATH_LITERAL("locale.pak"));
    EXPECT_EQ(base::WriteFile(locale_path, kEmptyPakContents, kEmptyPakSize),
              static_cast<int>(kEmptyPakSize));

    ui::ResourceBundle* resource_bundle = CreateResourceBundle(NULL);

    // Load the empty locale data pak.
    resource_bundle->LoadTestResources(base::FilePath(), locale_path);
    return resource_bundle;
  }

  // Returns the path of temporary directory to write test data packs into.
  const base::FilePath& dir_path() { return dir_.GetPath(); }

  // Returns the number of DataPacks managed by |resource_bundle|.
  size_t NumDataPacksInResourceBundle(ResourceBundle* resource_bundle) {
    DCHECK(resource_bundle);
    return resource_bundle->data_packs_.size();
  }

 private:
  std::unique_ptr<DataPack> locale_pack_;
  base::ScopedTempDir dir_;

  DISALLOW_COPY_AND_ASSIGN(ResourceBundleImageTest);
};

// Verify that we don't crash when trying to load a resource that is not found.
// In some cases, we fail to mmap resources.pak, but try to keep going anyway.
TEST_F(ResourceBundleImageTest, LoadDataResourceBytes) {
  base::FilePath data_path = dir_path().Append(FILE_PATH_LITERAL("sample.pak"));

  // Dump contents into the pak files.
  ASSERT_EQ(base::WriteFile(data_path, kEmptyPakContents,
      kEmptyPakSize), static_cast<int>(kEmptyPakSize));

  // Create a resource bundle from the file.
  ResourceBundle* resource_bundle = CreateResourceBundleWithEmptyLocalePak();
  resource_bundle->AddDataPackFromPath(data_path, SCALE_FACTOR_100P);

  const int kUnfoundResourceId = 10000;
  EXPECT_EQ(NULL, resource_bundle->LoadDataResourceBytes(
      kUnfoundResourceId));

  // Give a .pak file that doesn't exist so we will fail to load it.
  resource_bundle->AddDataPackFromPath(
      base::FilePath(FILE_PATH_LITERAL("non-existant-file.pak")),
      ui::SCALE_FACTOR_NONE);
  EXPECT_EQ(NULL, resource_bundle->LoadDataResourceBytes(
      kUnfoundResourceId));
}

TEST_F(ResourceBundleImageTest, GetRawDataResource) {
  base::FilePath data_path = dir_path().Append(FILE_PATH_LITERAL("sample.pak"));
  base::FilePath data_2x_path =
      dir_path().Append(FILE_PATH_LITERAL("sample_2x.pak"));

  // Dump contents into the pak files.
  ASSERT_EQ(base::WriteFile(data_path, kSamplePakContents,
      kSamplePakSize), static_cast<int>(kSamplePakSize));
  ASSERT_EQ(base::WriteFile(data_2x_path, kSamplePakContents2x,
      kSamplePakSize2x), static_cast<int>(kSamplePakSize2x));

  // Load the regular and 2x pak files.
  ResourceBundle* resource_bundle = CreateResourceBundleWithEmptyLocalePak();
  resource_bundle->AddDataPackFromPath(data_path, SCALE_FACTOR_100P);
  resource_bundle->AddDataPackFromPath(data_2x_path, SCALE_FACTOR_200P);

  // Resource ID 4 exists in both 1x and 2x paks, so we expect a different
  // result when requesting the 2x scale.
  EXPECT_EQ("this is id 4", resource_bundle->GetRawDataResourceForScale(4,
      SCALE_FACTOR_100P));
  EXPECT_EQ("this is id 4 2x", resource_bundle->GetRawDataResourceForScale(4,
      SCALE_FACTOR_200P));

  // Resource ID 6 only exists in the 1x pak so we expect the same resource
  // for both scale factor requests.
  EXPECT_EQ("this is id 6", resource_bundle->GetRawDataResourceForScale(6,
      SCALE_FACTOR_100P));
  EXPECT_EQ("this is id 6", resource_bundle->GetRawDataResourceForScale(6,
      SCALE_FACTOR_200P));
}

// Test requesting image reps at various scale factors from the image returned
// via ResourceBundle::GetImageNamed().
TEST_F(ResourceBundleImageTest, GetImageNamed) {
#if defined(OS_WIN)
  display::win::SetDefaultDeviceScaleFactor(2.0);
#endif
  std::vector<ScaleFactor> supported_factors;
  supported_factors.push_back(SCALE_FACTOR_100P);
  supported_factors.push_back(SCALE_FACTOR_200P);
  test::ScopedSetSupportedScaleFactors scoped_supported(supported_factors);
  base::FilePath data_1x_path = dir_path().AppendASCII("sample_1x.pak");
  base::FilePath data_2x_path = dir_path().AppendASCII("sample_2x.pak");

  // Create the pak files.
  CreateDataPackWithSingleBitmap(data_1x_path, 10, base::StringPiece());
  CreateDataPackWithSingleBitmap(data_2x_path, 20, base::StringPiece());

  // Load the regular and 2x pak files.
  ResourceBundle* resource_bundle = CreateResourceBundleWithEmptyLocalePak();
  resource_bundle->AddDataPackFromPath(data_1x_path, SCALE_FACTOR_100P);
  resource_bundle->AddDataPackFromPath(data_2x_path, SCALE_FACTOR_200P);

  EXPECT_EQ(SCALE_FACTOR_200P, resource_bundle->GetMaxScaleFactor());

  gfx::ImageSkia* image_skia = resource_bundle->GetImageSkiaNamed(3);

#if defined(OS_CHROMEOS) || defined(OS_WIN)
  // ChromeOS/Windows load highest scale factor first.
  EXPECT_EQ(ui::SCALE_FACTOR_200P,
            GetSupportedScaleFactor(image_skia->image_reps()[0].scale()));
#else
  EXPECT_EQ(ui::SCALE_FACTOR_100P,
            GetSupportedScaleFactor(image_skia->image_reps()[0].scale()));
#endif

  // Resource ID 3 exists in both 1x and 2x paks. Image reps should be
  // available for both scale factors in |image_skia|.
  gfx::ImageSkiaRep image_rep =
      image_skia->GetRepresentation(
      GetScaleForScaleFactor(ui::SCALE_FACTOR_100P));
  EXPECT_EQ(ui::SCALE_FACTOR_100P, GetSupportedScaleFactor(image_rep.scale()));
  image_rep =
      image_skia->GetRepresentation(
      GetScaleForScaleFactor(ui::SCALE_FACTOR_200P));
  EXPECT_EQ(ui::SCALE_FACTOR_200P, GetSupportedScaleFactor(image_rep.scale()));

  // The 1.4x pack was not loaded. Requesting the 1.4x resource should return
  // either the 1x or the 2x resource.
  image_rep = image_skia->GetRepresentation(
      ui::GetScaleForScaleFactor(ui::SCALE_FACTOR_140P));
  ui::ScaleFactor scale_factor = GetSupportedScaleFactor(image_rep.scale());
  EXPECT_TRUE(scale_factor == ui::SCALE_FACTOR_100P ||
              scale_factor == ui::SCALE_FACTOR_200P);

  // ImageSkia scales image if the one for the requested scale factor is not
  // available.
  EXPECT_EQ(1.4f, image_skia->GetRepresentation(1.4f).scale());
}

// Test that GetImageNamed() behaves properly for images which GRIT has
// annotated as having fallen back to 1x.
TEST_F(ResourceBundleImageTest, GetImageNamedFallback1x) {
  std::vector<ScaleFactor> supported_factors;
  supported_factors.push_back(SCALE_FACTOR_100P);
  supported_factors.push_back(SCALE_FACTOR_200P);
  test::ScopedSetSupportedScaleFactors scoped_supported(supported_factors);
  base::FilePath data_path = dir_path().AppendASCII("sample.pak");
  base::FilePath data_2x_path = dir_path().AppendASCII("sample_2x.pak");

  // Create the pak files.
  CreateDataPackWithSingleBitmap(data_path, 10, base::StringPiece());
  // 2x data pack bitmap has custom chunk to indicate that the 2x bitmap is not
  // available and that GRIT fell back to 1x.
  CreateDataPackWithSingleBitmap(data_2x_path, 10, base::StringPiece(
      reinterpret_cast<const char*>(kPngScaleChunk),
      arraysize(kPngScaleChunk)));

  // Load the regular and 2x pak files.
  ResourceBundle* resource_bundle = CreateResourceBundleWithEmptyLocalePak();
  resource_bundle->AddDataPackFromPath(data_path, SCALE_FACTOR_100P);
  resource_bundle->AddDataPackFromPath(data_2x_path, SCALE_FACTOR_200P);

  gfx::ImageSkia* image_skia = resource_bundle->GetImageSkiaNamed(3);

  // The image rep for 2x should be available. It should be resized to the
  // proper 2x size.
  gfx::ImageSkiaRep image_rep =
      image_skia->GetRepresentation(GetScaleForScaleFactor(
      ui::SCALE_FACTOR_200P));
  EXPECT_EQ(ui::SCALE_FACTOR_200P, GetSupportedScaleFactor(image_rep.scale()));
  EXPECT_EQ(20, image_rep.pixel_width());
  EXPECT_EQ(20, image_rep.pixel_height());
}

#if defined(OS_WIN)
// Tests GetImageNamed() behaves properly when the size of a scaled image
// requires rounding as a result of using a non-integer scale factor.
// Scale factors of 140 and 1805 are Windows specific.
TEST_F(ResourceBundleImageTest, GetImageNamedFallback1xRounding) {
  std::vector<ScaleFactor> supported_factors;
  supported_factors.push_back(SCALE_FACTOR_100P);
  supported_factors.push_back(SCALE_FACTOR_140P);
  supported_factors.push_back(SCALE_FACTOR_180P);
  test::ScopedSetSupportedScaleFactors scoped_supported(supported_factors);

  base::FilePath data_path = dir_path().AppendASCII("sample.pak");
  base::FilePath data_140P_path = dir_path().AppendASCII("sample_140P.pak");
  base::FilePath data_180P_path = dir_path().AppendASCII("sample_180P.pak");

  CreateDataPackWithSingleBitmap(data_path, 8, base::StringPiece());
  // Mark 140% and 180% images as requiring 1x fallback.
  CreateDataPackWithSingleBitmap(data_140P_path, 8, base::StringPiece(
    reinterpret_cast<const char*>(kPngScaleChunk),
    arraysize(kPngScaleChunk)));
  CreateDataPackWithSingleBitmap(data_180P_path, 8, base::StringPiece(
    reinterpret_cast<const char*>(kPngScaleChunk),
    arraysize(kPngScaleChunk)));

  ResourceBundle* resource_bundle = CreateResourceBundleWithEmptyLocalePak();
  resource_bundle->AddDataPackFromPath(data_path, SCALE_FACTOR_100P);
  resource_bundle->AddDataPackFromPath(data_140P_path, SCALE_FACTOR_140P);
  resource_bundle->AddDataPackFromPath(data_180P_path, SCALE_FACTOR_180P);

  // Non-integer dimensions should be rounded up.
  gfx::ImageSkia* image_skia = resource_bundle->GetImageSkiaNamed(3);
  gfx::ImageSkiaRep image_rep =
    image_skia->GetRepresentation(
    GetScaleForScaleFactor(ui::SCALE_FACTOR_140P));
  EXPECT_EQ(12, image_rep.pixel_width());
  image_rep = image_skia->GetRepresentation(
    GetScaleForScaleFactor(ui::SCALE_FACTOR_180P));
  EXPECT_EQ(15, image_rep.pixel_width());
}
#endif

#if defined(OS_IOS)
// Fails on devices that have non-100P scaling. See crbug.com/298406
#define MAYBE_FallbackToNone DISABLED_FallbackToNone
#else
#define MAYBE_FallbackToNone FallbackToNone
#endif
TEST_F(ResourceBundleImageTest, MAYBE_FallbackToNone) {
  base::FilePath data_default_path = dir_path().AppendASCII("sample.pak");

  // Create the pak files.
  CreateDataPackWithSingleBitmap(data_default_path, 10, base::StringPiece());

    // Load the regular pak files only.
  ResourceBundle* resource_bundle = CreateResourceBundleWithEmptyLocalePak();
  resource_bundle->AddDataPackFromPath(data_default_path, SCALE_FACTOR_NONE);

  gfx::ImageSkia* image_skia = resource_bundle->GetImageSkiaNamed(3);
  EXPECT_EQ(1u, image_skia->image_reps().size());
  EXPECT_TRUE(image_skia->image_reps()[0].unscaled());
  EXPECT_EQ(ui::SCALE_FACTOR_100P,
            GetSupportedScaleFactor(image_skia->image_reps()[0].scale()));
}

}  // namespace ui
