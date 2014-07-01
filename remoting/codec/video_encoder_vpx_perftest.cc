// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/codec/video_encoder_vpx.h"

#include <limits>
#include <vector>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "remoting/codec/codec_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"

using webrtc::DesktopSize;

namespace remoting {

// Measure the performance of the VP8 encoder.
TEST(VideoEncoderVpxTest, MeasureVp8Fps) {
  scoped_ptr<VideoEncoderVpx> encoder(VideoEncoderVpx::CreateForVP8());

  const DesktopSize kFrameSizes[] = {
    DesktopSize(1280, 1024), DesktopSize(1920, 1200)
  };

  for (size_t i = 0; i < arraysize(kFrameSizes); ++i) {
    float fps =
        MeasureVideoEncoderFpsWithSize(encoder.get(), kFrameSizes[i]);
    LOG(ERROR) << kFrameSizes[i].width() << "x" << kFrameSizes[i].height()
               << ": " << fps << "fps";
  }
}

// Measure the performance of the VP9 encoder.
TEST(VideoEncoderVpxTest, MeasureVp9Fps) {
  const DesktopSize kFrameSizes[] = {
    DesktopSize(1280, 1024), DesktopSize(1920, 1200)
  };

  for (int lossless_mode = 0; lossless_mode < 4; ++lossless_mode) {
    bool lossless_color = lossless_mode & 1;
    bool lossless_encode = lossless_mode & 2;

    scoped_ptr<VideoEncoderVpx> encoder(VideoEncoderVpx::CreateForVP9());
    encoder->SetLosslessColor(lossless_color);
    encoder->SetLosslessEncode(lossless_encode);

    for (size_t i = 0; i < arraysize(kFrameSizes); ++i) {
      float fps =
          MeasureVideoEncoderFpsWithSize(encoder.get(), kFrameSizes[i]);
      LOG(ERROR) << kFrameSizes[i].width() << "x" << kFrameSizes[i].height()
                 << "(" << (lossless_encode ? "lossless" : "lossy   ") << ")"
                 << "(" << (lossless_color ? "I444" : "I420") << ")"
                 << ": " << fps << "fps";
    }
  }
}

}  // namespace remoting
