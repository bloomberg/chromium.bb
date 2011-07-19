// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_CODEC_TEST_H_
#define REMOTING_BASE_CODEC_TEST_H_

#include "base/memory/ref_counted.h"
#include "media/base/video_frame.h"
#include "remoting/base/capture_data.h"

namespace remoting {

class Decoder;
class Encoder;

// Prepare testing data for encoding. Memory created is written to |memory|.
// Returns randomly generated data in CaptureData.
scoped_refptr<CaptureData> PrepareEncodeData(media::VideoFrame::Format format,
                                             uint8** memory);

// Generate test data and test the encoder for a regular encoding sequence.
// This will test encoder test and the sequence of messages sent.
//
// If |strict| is set to true then this routine will make sure the updated
// rects match dirty rects.
void TestEncoder(Encoder* encoder, bool strict);

// Generate test data and test the encoder and decoder pair.
//
// If |strict| is set to true, this routine will make sure the updated rects
// are correct.
void TestEncoderDecoder(Encoder* encoder, Decoder* decoder, bool strict);

}  // namespace remoting

#endif  // REMOTING_BASE_CODEC_TEST_H_
