// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/tools/test_shell/image_decoder_unittest.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/md5.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebData.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebImage.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebImageDecoder.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebSize.h"

using base::Time;

namespace {

const int kFirstFrameIndex = 0;

// Determine if we should test with file specified by |path| based
// on |file_selection| and the |threshold| for the file size.
bool ShouldSkipFile(const FilePath& path,
                    ImageDecoderTestFileSelection file_selection,
                    const int64 threshold) {
  if (file_selection == TEST_ALL)
    return false;

  int64 image_size = 0;
  file_util::GetFileSize(path, &image_size);
  return (file_selection == TEST_SMALLER) == (image_size > threshold);
}

}  // namespace

void ReadFileToVector(const FilePath& path, std::vector<char>* contents) {
  std::string raw_image_data;
  file_util::ReadFileToString(path, &raw_image_data);
  contents->resize(raw_image_data.size());
  memcpy(&contents->at(0), raw_image_data.data(), raw_image_data.size());
}

FilePath GetMD5SumPath(const FilePath& path) {
  static const FilePath::StringType kDecodedDataExtension(
      FILE_PATH_LITERAL(".md5sum"));
  return FilePath(path.value() + kDecodedDataExtension);
}

#if defined(CALCULATE_MD5_SUMS)
void SaveMD5Sum(const std::wstring& path, const WebKit::WebImage& web_image) {
  // Calculate MD5 sum.
  base::MD5Digest digest;
  web_image.getSkBitmap().lockPixels();
  base::MD5Sum(web_image.getSkBitmap().getPixels(),
         web_image.getSkBitmap().width() * web_image.getSkBitmap().height() *
         sizeof(uint32_t),
         &digest);

  // Write sum to disk.
  int bytes_written = file_util::WriteFile(path,
      reinterpret_cast<const char*>(&digest), sizeof digest);
  ASSERT_EQ(sizeof digest, bytes_written);
  web_image.getSkBitmap().unlockPixels();
}
#endif

#if !defined(CALCULATE_MD5_SUMS)
void VerifyImage(const WebKit::WebImageDecoder& decoder,
                 const FilePath& path,
                 const FilePath& md5_sum_path,
                 size_t frame_index) {
  // Make sure decoding can complete successfully.
  EXPECT_TRUE(decoder.isSizeAvailable()) << path.value();
  EXPECT_GE(decoder.frameCount(), frame_index) << path.value();
  EXPECT_TRUE(decoder.isFrameCompleteAtIndex(frame_index)) << path.value();
  EXPECT_FALSE(decoder.isFailed());

  // Calculate MD5 sum.
  base::MD5Digest actual_digest;
  WebKit::WebImage web_image = decoder.getFrameAtIndex(frame_index);
  web_image.getSkBitmap().lockPixels();
  base::MD5Sum(web_image.getSkBitmap().getPixels(),
         web_image.getSkBitmap().width() * web_image.getSkBitmap().height() *
         sizeof(uint32_t),
         &actual_digest);

  // Read the MD5 sum off disk.
  std::string file_bytes;
  file_util::ReadFileToString(md5_sum_path, &file_bytes);
  base::MD5Digest expected_digest;
  ASSERT_EQ(sizeof expected_digest, file_bytes.size()) << path.value();
  memcpy(&expected_digest, file_bytes.data(), sizeof expected_digest);

  // Verify that the sums are the same.
  EXPECT_EQ(0,
            memcmp(&expected_digest, &actual_digest, sizeof(base::MD5Digest)))
    << path.value();
  web_image.getSkBitmap().unlockPixels();
}
#endif

void ImageDecoderTest::SetUp() {
  FilePath data_dir;
  ASSERT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &data_dir));
  data_dir_ = data_dir.AppendASCII("webkit").
                       AppendASCII("data").
                       AppendASCII(format_ + "_decoder");
  ASSERT_TRUE(file_util::PathExists(data_dir_)) << data_dir_.value();
}

std::vector<FilePath> ImageDecoderTest::GetImageFiles() const {
  std::string pattern = "*." + format_;

  file_util::FileEnumerator enumerator(data_dir_,
                                       false,
                                       file_util::FileEnumerator::FILES);

  std::vector<FilePath> image_files;
  FilePath next_file_name;
  while (!(next_file_name = enumerator.Next()).empty()) {
    FilePath base_name = next_file_name.BaseName();
#if defined(OS_WIN)
    std::string base_name_ascii = WideToASCII(base_name.value());
#else
    std::string base_name_ascii = base_name.value();
#endif
    if (!MatchPattern(base_name_ascii, pattern))
      continue;
    image_files.push_back(next_file_name);
  }

  return image_files;
}

bool ImageDecoderTest::ShouldImageFail(const FilePath& path) const {
  static const FilePath::StringType kBadSuffix(FILE_PATH_LITERAL(".bad."));
  return (path.value().length() > (kBadSuffix.length() + format_.length()) &&
          !path.value().compare(path.value().length() - format_.length() -
                                    kBadSuffix.length(),
                                kBadSuffix.length(), kBadSuffix));
}

void ImageDecoderTest::TestDecoding(
    ImageDecoderTestFileSelection file_selection,
    const int64 threshold) {
  const std::vector<FilePath> image_files(GetImageFiles());
  for (std::vector<FilePath>::const_iterator i = image_files.begin();
       i != image_files.end(); ++i) {
    if (ShouldSkipFile(*i, file_selection, threshold))
      continue;
    const FilePath md5_sum_path(GetMD5SumPath(*i));
    TestWebKitImageDecoder(*i, md5_sum_path, kFirstFrameIndex);
  }
}

void  ImageDecoderTest::TestWebKitImageDecoder(const FilePath& image_path,
  const FilePath& md5_sum_path, int desired_frame_index) const {
  bool should_test_chunking = true;
  bool should_test_failed_images = true;
#ifdef CALCULATE_MD5_SUMS
    // Do not test anything just get the md5 sums.
    should_test_chunking = false;
    should_test_failed_images = false;
#endif

  std::vector<char> image_contents;
  ReadFileToVector(image_path, &image_contents);
  EXPECT_TRUE(image_contents.size());
  scoped_ptr<WebKit::WebImageDecoder> decoder(CreateWebKitImageDecoder());
  EXPECT_FALSE(decoder->isFailed());

  if (should_test_chunking) {
    // Test chunking file into half.
    const int partial_size = image_contents.size()/2;

    WebKit::WebData partial_data(
      reinterpret_cast<const char*>(&(image_contents.at(0))), partial_size);

    // Make Sure the image decoder doesn't fail when we ask for the frame
    // buffer for this partial image.
    // NOTE: We can't check that frame 0 is non-NULL, because if this is an
    // ICO and we haven't yet supplied enough data to read the directory,
    // there is no framecount and thus no first frame.
    decoder->setData(const_cast<WebKit::WebData&>(partial_data), false);
    EXPECT_FALSE(decoder->isFailed()) << image_path.value();
  }

  // Make sure passing the complete image results in successful decoding.
  WebKit::WebData data(reinterpret_cast<const char*>(&(image_contents.at(0))),
    image_contents.size());
  decoder->setData(const_cast<WebKit::WebData&>(data), true);

  if (should_test_failed_images) {
    if (ShouldImageFail(image_path)) {
      EXPECT_FALSE(decoder->isFrameCompleteAtIndex(kFirstFrameIndex));
      EXPECT_TRUE(decoder->isFailed());
         return;
    }
  }

  EXPECT_FALSE(decoder->isFailed()) << image_path.value();

#ifdef CALCULATE_MD5_SUMS
  // Since WebImage does not expose get data by frame, get the size
  // through decoder and pass it to fromData so that the closest
  // image dats to the size is returned.
  WebKit::WebSize size(decoder->getImage(desired_frame_index).size());
  const WebKit::WebImage& image = WebKit::WebImage::fromData(data, size);
  SaveMD5Sum(md5_sum_path.value(), image);
#else
  VerifyImage(*decoder, image_path, md5_sum_path, desired_frame_index);
#endif
}
