// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TestMediaStreamClient is an implementation of webkit_media::MediaStreamClient
// and used with WebKit::WebUserMediaClientMock to provide corresponding video
// decoder to media pipeline.

#ifndef WEBKIT_MOCKS_TEST_MEDIA_STREAM_CLIENT_H_
#define WEBKIT_MOCKS_TEST_MEDIA_STREAM_CLIENT_H_

#include "base/callback_forward.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebURL.h"
#include "webkit/media/media_stream_client.h"

namespace WebKit {
class WebFrame;
class WebMediaPlayer;
class WebMediaPlayerClient;
}

namespace webkit_media {
class MediaStreamAudioRenderer;
class MediaStreamClient;
}

namespace webkit_glue {

// This is used by WebFrameClient::createMediaPlayer().
WebKit::WebMediaPlayer* CreateMediaPlayer(
    WebKit::WebFrame* frame,
    const WebKit::WebURL& url,
    WebKit::WebMediaPlayerClient* client,
    webkit_media::MediaStreamClient* media_stream_client);

class TestMediaStreamClient : public webkit_media::MediaStreamClient {
 public:
  TestMediaStreamClient();
  virtual ~TestMediaStreamClient();

  // Implement webkit_media::MediaStreamClient.
  virtual bool IsMediaStream(const GURL& url) OVERRIDE;
  virtual scoped_refptr<webkit_media::VideoFrameProvider> GetVideoFrameProvider(
      const GURL& url,
      const base::Closure& error_cb,
      const webkit_media::VideoFrameProvider::RepaintCB& repaint_cb) OVERRIDE;
  virtual scoped_refptr<webkit_media::MediaStreamAudioRenderer>
      GetAudioRenderer(const GURL& url) OVERRIDE;
};

}  // namespace webkit_glue

#endif  // WEBKIT_MOCKS_TEST_MEDIA_STREAM_CLIENT_H_
