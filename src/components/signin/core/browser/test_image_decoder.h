// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_TEST_IMAGE_DECODER_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_TEST_IMAGE_DECODER_H_

#include <memory>

#include "base/macros.h"
#include "build/build_config.h"
#include "components/image_fetcher/core/image_decoder.h"
#include "components/signin/core/browser/account_fetcher_service.h"

// This dummy class implements |image_fetcher::ImageDecoder|, and is passed
// as an argument to |AccountFetcherService::Initialize|.
class TestImageDecoder : public image_fetcher::ImageDecoder {
 public:
  TestImageDecoder();
  ~TestImageDecoder() override;

  // image_fetcher::Decoder implementation:

  // If |image_data| is non-empty, a blank 64x64 image is passed to callback.
  // Otherwise an empty image is passed.
  void DecodeImage(
      const std::string& image_data,
      const gfx::Size& desired_image_frame_size,
      const image_fetcher::ImageDecodedCallback& callback) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestImageDecoder);
};

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_TEST_IMAGE_DECODER_H_
