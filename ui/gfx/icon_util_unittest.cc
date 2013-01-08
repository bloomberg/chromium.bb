// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/gfx_paths.h"
#include "ui/gfx/icon_util.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/size.h"

namespace {

static const char kSmallIconName[] = "icon_util/16_X_16_icon.ico";
static const char kLargeIconName[] = "icon_util/128_X_128_icon.ico";
static const char kTempIconFilename[] = "temp_test_icon.ico";

class IconUtilTest : public testing::Test {
 public:
  IconUtilTest() {
    PathService::Get(gfx::DIR_TEST_DATA, &test_data_directory_);
  }
  ~IconUtilTest() {}

  static const int kSmallIconWidth = 16;
  static const int kSmallIconHeight = 16;
  static const int kLargeIconWidth = 128;
  static const int kLargeIconHeight = 128;

  // Given a file name for an .ico file and an image dimentions, this
  // function loads the icon and returns an HICON handle.
  HICON LoadIconFromFile(const FilePath& filename, int width, int height) {
    HICON icon = static_cast<HICON>(LoadImage(NULL,
                                    filename.value().c_str(),
                                    IMAGE_ICON,
                                    width,
                                    height,
                                    LR_LOADTRANSPARENT | LR_LOADFROMFILE));
    return icon;
  }

  SkBitmap CreateBlackSkBitmap(int width, int height) {
    SkBitmap bitmap;
    bitmap.setConfig(SkBitmap::kARGB_8888_Config, width, height);
    bitmap.allocPixels();
    // Setting the pixels to black.
    memset(bitmap.getPixels(), 0, width * height * 4);
    return bitmap;
  }

 protected:
  // The root directory for test files.
  FilePath test_data_directory_;

  // Directory for creating files by this test.
  base::ScopedTempDir temp_directory_;

 private:
  DISALLOW_COPY_AND_ASSIGN(IconUtilTest);
};

}  // namespace

// The following test case makes sure IconUtil::SkBitmapFromHICON fails
// gracefully when called with invalid input parameters.
TEST_F(IconUtilTest, TestIconToBitmapInvalidParameters) {
  FilePath icon_filename = test_data_directory_.AppendASCII(kSmallIconName);
  gfx::Size icon_size(kSmallIconWidth, kSmallIconHeight);
  HICON icon = LoadIconFromFile(icon_filename,
                                icon_size.width(),
                                icon_size.height());
  ASSERT_TRUE(icon != NULL);

  // Invalid size parameter.
  gfx::Size invalid_icon_size(kSmallIconHeight, 0);
  EXPECT_EQ(IconUtil::CreateSkBitmapFromHICON(icon, invalid_icon_size),
            static_cast<SkBitmap*>(NULL));

  // Invalid icon.
  EXPECT_EQ(IconUtil::CreateSkBitmapFromHICON(NULL, icon_size),
            static_cast<SkBitmap*>(NULL));

  // The following code should succeed.
  scoped_ptr<SkBitmap> bitmap;
  bitmap.reset(IconUtil::CreateSkBitmapFromHICON(icon, icon_size));
  EXPECT_NE(bitmap.get(), static_cast<SkBitmap*>(NULL));
  ::DestroyIcon(icon);
}

// The following test case makes sure IconUtil::CreateHICONFromSkBitmap fails
// gracefully when called with invalid input parameters.
TEST_F(IconUtilTest, TestBitmapToIconInvalidParameters) {
  HICON icon = NULL;
  scoped_ptr<SkBitmap> bitmap;

  // Wrong bitmap format.
  bitmap.reset(new SkBitmap);
  ASSERT_NE(bitmap.get(), static_cast<SkBitmap*>(NULL));
  bitmap->setConfig(SkBitmap::kA8_Config, kSmallIconWidth, kSmallIconHeight);
  icon = IconUtil::CreateHICONFromSkBitmap(*bitmap);
  EXPECT_EQ(icon, static_cast<HICON>(NULL));

  // Invalid bitmap size.
  bitmap.reset(new SkBitmap);
  ASSERT_NE(bitmap.get(), static_cast<SkBitmap*>(NULL));
  bitmap->setConfig(SkBitmap::kARGB_8888_Config, 0, 0);
  icon = IconUtil::CreateHICONFromSkBitmap(*bitmap);
  EXPECT_EQ(icon, static_cast<HICON>(NULL));

  // Valid bitmap configuration but no pixels allocated.
  bitmap.reset(new SkBitmap);
  ASSERT_NE(bitmap.get(), static_cast<SkBitmap*>(NULL));
  bitmap->setConfig(SkBitmap::kARGB_8888_Config,
                    kSmallIconWidth,
                    kSmallIconHeight);
  icon = IconUtil::CreateHICONFromSkBitmap(*bitmap);
  EXPECT_TRUE(icon == NULL);
}

// The following test case makes sure IconUtil::CreateIconFileFromSkBitmap
// fails gracefully when called with invalid input parameters.
TEST_F(IconUtilTest, TestCreateIconFileInvalidParameters) {
  scoped_ptr<SkBitmap> bitmap;
  FilePath valid_icon_filename = test_data_directory_.AppendASCII(
      kSmallIconName);
  FilePath invalid_icon_filename(FILE_PATH_LITERAL("C:\\<>?.ico"));

  // Wrong bitmap format.
  bitmap.reset(new SkBitmap);
  ASSERT_NE(bitmap.get(), static_cast<SkBitmap*>(NULL));
  bitmap->setConfig(SkBitmap::kA8_Config, kSmallIconWidth, kSmallIconHeight);
  EXPECT_FALSE(IconUtil::CreateIconFileFromSkBitmap(*bitmap, SkBitmap(),
                                                    valid_icon_filename));

  // Invalid bitmap size.
  bitmap.reset(new SkBitmap);
  ASSERT_NE(bitmap.get(), static_cast<SkBitmap*>(NULL));
  bitmap->setConfig(SkBitmap::kARGB_8888_Config, 0, 0);
  EXPECT_FALSE(IconUtil::CreateIconFileFromSkBitmap(*bitmap, SkBitmap(),
                                                    valid_icon_filename));

  // Bitmap with no allocated pixels.
  bitmap.reset(new SkBitmap);
  ASSERT_NE(bitmap.get(), static_cast<SkBitmap*>(NULL));
  bitmap->setConfig(SkBitmap::kARGB_8888_Config,
                    kSmallIconWidth,
                    kSmallIconHeight);
  EXPECT_FALSE(IconUtil::CreateIconFileFromSkBitmap(*bitmap, SkBitmap(),
                                                    valid_icon_filename));

  // Invalid file name.
  bitmap->allocPixels();
  // Setting the pixels to black.
  memset(bitmap->getPixels(), 0, bitmap->width() * bitmap->height() * 4);
  EXPECT_FALSE(IconUtil::CreateIconFileFromSkBitmap(*bitmap, SkBitmap(),
                                                    invalid_icon_filename));
}

// This test case makes sure that when we load an icon from disk and convert
// the HICON into a bitmap, the bitmap has the expected format and dimentions.
TEST_F(IconUtilTest, TestCreateSkBitmapFromHICON) {
  scoped_ptr<SkBitmap> bitmap;
  FilePath small_icon_filename = test_data_directory_.AppendASCII(
      kSmallIconName);
  gfx::Size small_icon_size(kSmallIconWidth, kSmallIconHeight);
  HICON small_icon = LoadIconFromFile(small_icon_filename,
                                      small_icon_size.width(),
                                      small_icon_size.height());
  ASSERT_NE(small_icon, static_cast<HICON>(NULL));
  bitmap.reset(IconUtil::CreateSkBitmapFromHICON(small_icon, small_icon_size));
  ASSERT_NE(bitmap.get(), static_cast<SkBitmap*>(NULL));
  EXPECT_EQ(bitmap->width(), small_icon_size.width());
  EXPECT_EQ(bitmap->height(), small_icon_size.height());
  EXPECT_EQ(bitmap->config(), SkBitmap::kARGB_8888_Config);
  ::DestroyIcon(small_icon);

  FilePath large_icon_filename = test_data_directory_.AppendASCII(
      kLargeIconName);
  gfx::Size large_icon_size(kLargeIconWidth, kLargeIconHeight);
  HICON large_icon = LoadIconFromFile(large_icon_filename,
                                      large_icon_size.width(),
                                      large_icon_size.height());
  ASSERT_NE(large_icon, static_cast<HICON>(NULL));
  bitmap.reset(IconUtil::CreateSkBitmapFromHICON(large_icon, large_icon_size));
  ASSERT_NE(bitmap.get(), static_cast<SkBitmap*>(NULL));
  EXPECT_EQ(bitmap->width(), large_icon_size.width());
  EXPECT_EQ(bitmap->height(), large_icon_size.height());
  EXPECT_EQ(bitmap->config(), SkBitmap::kARGB_8888_Config);
  ::DestroyIcon(large_icon);
}

// This test case makes sure that when an HICON is created from an SkBitmap,
// the returned handle is valid and refers to an icon with the expected
// dimentions color depth etc.
TEST_F(IconUtilTest, TestBasicCreateHICONFromSkBitmap) {
  SkBitmap bitmap = CreateBlackSkBitmap(kSmallIconWidth, kSmallIconHeight);
  HICON icon = IconUtil::CreateHICONFromSkBitmap(bitmap);
  EXPECT_NE(icon, static_cast<HICON>(NULL));
  ICONINFO icon_info;
  ASSERT_TRUE(::GetIconInfo(icon, &icon_info));
  EXPECT_TRUE(icon_info.fIcon);

  // Now that have the icon information, we should obtain the specification of
  // the icon's bitmap and make sure it matches the specification of the
  // SkBitmap we started with.
  //
  // The bitmap handle contained in the icon information is a handle to a
  // compatible bitmap so we need to call ::GetDIBits() in order to retrieve
  // the bitmap's header information.
  BITMAPINFO bitmap_info;
  ::ZeroMemory(&bitmap_info, sizeof(BITMAPINFO));
  bitmap_info.bmiHeader.biSize = sizeof(BITMAPINFO);
  HDC hdc = ::GetDC(NULL);
  int result = ::GetDIBits(hdc,
                           icon_info.hbmColor,
                           0,
                           kSmallIconWidth,
                           NULL,
                           &bitmap_info,
                           DIB_RGB_COLORS);
  ASSERT_GT(result, 0);
  EXPECT_EQ(bitmap_info.bmiHeader.biWidth, kSmallIconWidth);
  EXPECT_EQ(bitmap_info.bmiHeader.biHeight, kSmallIconHeight);
  EXPECT_EQ(bitmap_info.bmiHeader.biPlanes, 1);
  EXPECT_EQ(bitmap_info.bmiHeader.biBitCount, 32);
  ::ReleaseDC(NULL, hdc);
  ::DestroyIcon(icon);
}

// The following test case makes sure IconUtil::CreateIconFileFromSkBitmap
// creates a valid .ico file given an SkBitmap.
TEST_F(IconUtilTest, TestCreateIconFile) {
  FilePath icon_filename = test_data_directory_.AppendASCII(kTempIconFilename);

  SkBitmap bitmap = CreateBlackSkBitmap(kSmallIconWidth, kSmallIconHeight);
  EXPECT_TRUE(IconUtil::CreateIconFileFromSkBitmap(bitmap, SkBitmap(),
                                                   icon_filename));

  // We are currently only testing that it is possible to load an icon from
  // the .ico file we just created. We don't really check the additional icon
  // images created by IconUtil::CreateIconFileFromSkBitmap.
  HICON icon = LoadIconFromFile(icon_filename,
                                kSmallIconWidth,
                                kSmallIconHeight);
  EXPECT_NE(icon, static_cast<HICON>(NULL));
  if (icon != NULL) {
    ::DestroyIcon(icon);
  }
}

TEST_F(IconUtilTest, TestCreateIconFileWithLargeBitmap) {
  const FilePath icon_path(temp_directory_.path().AppendASCII("test.ico"));
  const SkBitmap bitmap_48 = CreateBlackSkBitmap(48, 48);
  const SkBitmap bitmap_256 = CreateBlackSkBitmap(256, 256);

  // First, create the icon file.
  ASSERT_TRUE(IconUtil::CreateIconFileFromSkBitmap(bitmap_48, bitmap_256,
                                                   icon_path));
  ASSERT_TRUE(file_util::PathExists(icon_path));

  // Then, read the file and ensure it has a valid 256x256 PNG icon entry.
  std::string icon_data;
  ASSERT_TRUE(file_util::ReadFileToString(icon_path, &icon_data));
  ASSERT_GE(icon_data.length(), sizeof(IconUtil::ICONDIR));

  const IconUtil::ICONDIR* icon_dir =
      reinterpret_cast<const IconUtil::ICONDIR*>(icon_data.data());
  ASSERT_GE(icon_data.length(),
            sizeof(IconUtil::ICONDIR) +
                icon_dir->idCount * sizeof(IconUtil::ICONDIRENTRY));
  const IconUtil::ICONDIRENTRY* png_entry = NULL;
  for (size_t i = 0; i < icon_dir->idCount; ++i) {
    const IconUtil::ICONDIRENTRY* entry = &icon_dir->idEntries[i];
    if (entry->bWidth == 0 && entry->bHeight == 0) {
      EXPECT_EQ(NULL, png_entry);
      png_entry = entry;
    }
  }
  ASSERT_TRUE(png_entry);

  // Convert the PNG entry data back to a SkBitmap to ensure it's valid.
  ASSERT_GE(icon_data.length(),
            png_entry->dwImageOffset + png_entry->dwBytesInRes);
  const unsigned char* png_bytes = reinterpret_cast<const unsigned char*>(
      icon_data.data() + png_entry->dwImageOffset);
  gfx::Image image = gfx::Image::CreateFrom1xPNGBytes(png_bytes,
                                                      png_entry->dwBytesInRes);
  SkBitmap bitmap = image.AsBitmap();
  EXPECT_EQ(256, bitmap.width());
  EXPECT_EQ(256, bitmap.height());
}
