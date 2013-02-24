// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/file_util.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebImageDecoder.h"
#include "webkit/tools/test_shell/image_decoder_unittest.h"

using WebKit::WebImageDecoder;

class ICOImageDecoderTest : public ImageDecoderTest {
 public:
  ICOImageDecoderTest() : ImageDecoderTest("ico") { }

 protected:
   virtual WebKit::WebImageDecoder* CreateWebKitImageDecoder() const OVERRIDE {
     return new WebKit::WebImageDecoder(WebKit::WebImageDecoder::TypeICO);
  }
};

TEST_F(ICOImageDecoderTest, Decoding) {
  TestDecoding();
}

TEST_F(ICOImageDecoderTest, ImageNonZeroFrameIndex) {
  // Test that the decoder decodes multiple sizes of icons which have them.
  // Load an icon that has both favicon-size and larger entries.
  base::FilePath multisize_icon_path(data_dir_.AppendASCII("yahoo.ico"));
  const base::FilePath md5_sum_path(
      GetMD5SumPath(multisize_icon_path).value() + FILE_PATH_LITERAL("2"));
  static const int kDesiredFrameIndex = 3;
  TestWebKitImageDecoder(multisize_icon_path, md5_sum_path, kDesiredFrameIndex);
}
