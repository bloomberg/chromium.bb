// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/support/test_media_stream_client.h"

#include "googleurl/src/gurl.h"
#include "media/base/message_loop_factory.h"
#include "media/base/pipeline.h"
#include "media/filters/video_frame_generator.h"

#include "third_party/WebKit/Source/WebKit/chromium/public/WebMediaStreamRegistry.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebMediaStreamComponent.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebMediaStreamDescriptor.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebVector.h"

using namespace WebKit;

namespace {

static const int kVideoCaptureWidth = 352;
static const int kVideoCaptureHeight = 288;
static const int kVideoCaptureFrameDurationMs = 33;

bool IsMockMediaStreamWithVideo(const WebURL& url) {
  WebMediaStreamDescriptor descriptor(
      WebMediaStreamRegistry::lookupMediaStreamDescriptor(url));
  if (descriptor.isNull())
    return false;
  WebVector<WebMediaStreamComponent> videoSources;
  descriptor.videoSources(videoSources);
  return videoSources.size() > 0;
}

}  // namespace

namespace webkit_support {

scoped_refptr<media::VideoDecoder> TestMediaStreamClient::GetVideoDecoder(
    const GURL& url, media::MessageLoopFactory* message_loop_factory) {
  // This class is installed in a chain of possible VideoDecoder creators
  // which are called in order until one returns an object.
  // Make sure we are dealing with a Mock MediaStream. If not, bail out.
  if (!IsMockMediaStreamWithVideo(url))
    return NULL;

  return new media::VideoFrameGenerator(
      message_loop_factory->GetMessageLoop(media::MessageLoopFactory::kDecoder),
      gfx::Size(kVideoCaptureWidth, kVideoCaptureHeight),
      base::TimeDelta::FromMilliseconds(kVideoCaptureFrameDurationMs));
}

}  // namespace webkit_support
