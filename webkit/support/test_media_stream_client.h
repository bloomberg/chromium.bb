// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TestMediaStreamClient is an implementation of webkit_media::MediaStreamClient
// and used with WebKit::WebUserMediaClientMock to provide corresponding video
// decoder to media pipeline.

#ifndef WEBKIT_SUPPORT_TEST_MEDIA_STREAM_CLIENT_H_
#define WEBKIT_SUPPORT_TEST_MEDIA_STREAM_CLIENT_H_

#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebURL.h"
#include "webkit/media/media_stream_client.h"

namespace webkit_support {

class MediaStreamUtil {
 public:
  virtual bool IsMockStream(const WebKit::WebURL& url) = 0;
};

class TestMediaStreamClient : public webkit_media::MediaStreamClient {
 public:
  explicit TestMediaStreamClient(MediaStreamUtil* media_stream_util);
  virtual ~TestMediaStreamClient() {}

  // Implement webkit_media::MediaStreamClient.
  virtual scoped_refptr<media::VideoDecoder> GetVideoDecoder(
      const GURL& url,
      media::MessageLoopFactory* message_loop_factory) OVERRIDE;

 private:
  MediaStreamUtil* media_stream_util_;
};

}  // namespace webkit_support

#endif  // WEBKIT_SUPPORT_TEST_MEDIA_STREAM_CLIENT_H_
