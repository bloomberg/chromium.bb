// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_TOOLS_TEST_SHELL_IMAGE_DECODER_UNITTEST_H_
#define WEBKIT_TOOLS_TEST_SHELL_IMAGE_DECODER_UNITTEST_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace WebKit { class WebImageDecoder; }

// If CALCULATE_MD5_SUMS is not defined, then this test decodes a handful of
// image files and compares their MD5 sums to the stored sums on disk.
//
// To recalculate the MD5 sums, uncommment CALCULATE_MD5_SUMS.
//
// The image files and corresponding MD5 sums live in the directory
// chrome/test/data/*_decoder (where "*" is the format being tested).
//
// Note: The MD5 sums calculated in this test by little- and big-endian systems
// will differ, since no endianness correction is done.  If we start compiling
// for big endian machines this should be fixed.

// #define CALCULATE_MD5_SUMS

enum ImageDecoderTestFileSelection {
  TEST_ALL,
  TEST_SMALLER,
  TEST_BIGGER,
};

// Returns the path the decoded data is saved at.
base::FilePath GetMD5SumPath(const base::FilePath& path);

class ImageDecoderTest : public testing::Test {
 public:
  explicit ImageDecoderTest(const std::string& format) : format_(format) { }

 protected:
  virtual void SetUp() OVERRIDE;

  // Returns the vector of image files for testing.
  std::vector<base::FilePath> GetImageFiles() const;

  // Returns true if the image is bogus and should not be successfully decoded.
  bool ShouldImageFail(const base::FilePath& path) const;

  // Tests if decoder decodes image at image_path with underlying frame at
  // index desired_frame_index. The md5_sum_path is needed if the test is not
  // asked to generate one i.e. if # #define CALCULATE_MD5_SUMS is set.
  void TestWebKitImageDecoder(const base::FilePath& image_path,
    const base::FilePath& md5_sum_path, int desired_frame_index) const;

  // Verifies each of the test image files is decoded correctly and matches the
  // expected state. |file_selection| and |threshold| can be used to select
  // files to test based on file size.
  // If just the MD5 sum is wanted, this skips chunking.
  void TestDecoding(ImageDecoderTestFileSelection file_selection,
                    const int64 threshold);

  void TestDecoding()  {
    TestDecoding(TEST_ALL, 0);
  }

  // Creates WebKit API's decoder.
  virtual WebKit::WebImageDecoder* CreateWebKitImageDecoder() const = 0;

  // The format to be decoded, like "bmp" or "ico".
  std::string format_;

 protected:
  // Path to the test files.
  base::FilePath data_dir_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ImageDecoderTest);
};

#endif  // WEBKIT_TOOLS_TEST_SHELL_IMAGE_DECODER_UNITTEST_H_
