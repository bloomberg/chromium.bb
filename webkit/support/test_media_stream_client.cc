// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/support/test_media_stream_client.h"

#include "googleurl/src/gurl.h"
#include "media/base/message_loop_factory.h"
#include "media/base/pipeline.h"
#include "media/filters/video_frame_generator.h"

namespace {

static const int kVideoCaptureWidth = 352;
static const int kVideoCaptureHeight = 288;
static const int kVideoCaptureFrameDurationMs = 33;

}  // namespace

namespace webkit_support {

TestMediaStreamClient::TestMediaStreamClient(MediaStreamUtil* media_stream_util)
    : media_stream_util_(media_stream_util) {
}

scoped_refptr<media::VideoDecoder> TestMediaStreamClient::GetVideoDecoder(
    const GURL& url, media::MessageLoopFactory* message_loop_factory) {
  if (!media_stream_util_ || !media_stream_util_->IsMockStream(url))
    return NULL;

  return new media::VideoFrameGenerator(
      message_loop_factory->GetMessageLoopProxy("CaptureVideoDecoder").get(),
      gfx::Size(kVideoCaptureWidth, kVideoCaptureHeight),
      base::TimeDelta::FromMilliseconds(kVideoCaptureFrameDurationMs));
}

}  // namespace webkit_support
