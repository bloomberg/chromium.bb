// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/support/test_media_stream_client.h"

#include "googleurl/src/gurl.h"
#include "media/base/pipeline.h"
#include "media/filters/video_frame_generator.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebMediaStreamComponent.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebMediaStreamDescriptor.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebVector.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebMediaStreamRegistry.h"
#include "webkit/media/media_stream_audio_renderer.h"
#include "webkit/media/simple_video_frame_provider.h"

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

TestMediaStreamClient::TestMediaStreamClient() {}

TestMediaStreamClient::~TestMediaStreamClient() {}

bool TestMediaStreamClient::IsMediaStream(const GURL& url) {
  return IsMockMediaStreamWithVideo(url);
}

scoped_refptr<webkit_media::VideoFrameProvider>
TestMediaStreamClient::GetVideoFrameProvider(
    const GURL& url,
    const base::Closure& error_cb,
    const webkit_media::VideoFrameProvider::RepaintCB& repaint_cb) {
  if (!IsMockMediaStreamWithVideo(url))
    return NULL;

  return new webkit_media::SimpleVideoFrameProvider(
      gfx::Size(kVideoCaptureWidth, kVideoCaptureHeight),
      base::TimeDelta::FromMilliseconds(kVideoCaptureFrameDurationMs),
      error_cb,
      repaint_cb);
}

scoped_refptr<media::VideoDecoder> TestMediaStreamClient::GetVideoDecoder(
    const GURL& url,
    const scoped_refptr<base::MessageLoopProxy>& message_loop) {
  // This class is installed in a chain of possible VideoDecoder creators
  // which are called in order until one returns an object.
  // Make sure we are dealing with a Mock MediaStream. If not, bail out.
  if (!IsMockMediaStreamWithVideo(url))
    return NULL;

  return new media::VideoFrameGenerator(
      message_loop,
      gfx::Size(kVideoCaptureWidth, kVideoCaptureHeight),
      base::TimeDelta::FromMilliseconds(kVideoCaptureFrameDurationMs));
}

scoped_refptr<webkit_media::MediaStreamAudioRenderer>
TestMediaStreamClient::GetAudioRenderer(const GURL& url) {
  return NULL;
}

}  // namespace webkit_support
