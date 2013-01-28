// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CODEC_CODEC_TEST_H_
#define REMOTING_CODEC_CODEC_TEST_H_

#include "base/memory/ref_counted.h"
#include "media/base/video_frame.h"
#include "media/video/capture/screen/screen_capture_data.h"

namespace remoting {

class VideoDecoder;
class VideoEncoder;

// Generate test data and test the encoder for a regular encoding sequence.
// This will test encoder test and the sequence of messages sent.
//
// If |strict| is set to true then this routine will make sure the updated
// rects match dirty rects.
void TestVideoEncoder(VideoEncoder* encoder, bool strict);

// Generate test data and test the encoder and decoder pair.
//
// If |strict| is set to true, this routine will make sure the updated rects
// are correct.
void TestVideoEncoderDecoder(VideoEncoder* encoder,
                             VideoDecoder* decoder,
                             bool strict);

// Generate a frame containing a gradient, and test the encoder and decoder
// pair.
void TestVideoEncoderDecoderGradient(VideoEncoder* encoder,
                                     VideoDecoder* decoder,
                                     const SkISize& screen_size,
                                     const SkISize& view_size,
                                     double max_error_limit,
                                     double mean_error_limit);

}  // namespace remoting

#endif  // REMOTING_CODEC_CODEC_TEST_H_
