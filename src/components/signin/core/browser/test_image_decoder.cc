// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/test_image_decoder.h"

#include "base/values.h"
#include "build/build_config.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "ui/gfx/image/image_unittest_util.h"

TestImageDecoder::TestImageDecoder() = default;

TestImageDecoder::~TestImageDecoder() = default;

void TestImageDecoder::DecodeImage(
    const std::string& image_data,
    const gfx::Size& desired_image_frame_size,
    const image_fetcher::ImageDecodedCallback& callback) {
  callback.Run(image_data.empty() ? gfx::Image()
                                  : gfx::test::CreateImage(64, 64));
}
