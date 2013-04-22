// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_MEDIA_WEBMEDIASOURCECLIENT_IMPL_H_
#define WEBKIT_MEDIA_WEBMEDIASOURCECLIENT_IMPL_H_

#include <string>
#include <vector>

#include "media/base/media_log.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebMediaSourceClient.h"

namespace media {
class ChunkDemuxer;
}

namespace webkit_media {

class WebMediaSourceClientImpl : public WebKit::WebMediaSourceClient {
 public:
  WebMediaSourceClientImpl(media::ChunkDemuxer* demuxer, media::LogCB log_cb);
  virtual ~WebMediaSourceClientImpl();

  // WebKit::WebMediaSourceClient implementation.
  virtual AddStatus addSourceBuffer(
      const WebKit::WebString& type,
      const WebKit::WebVector<WebKit::WebString>& codecs,
      WebKit::WebSourceBuffer** source_buffer) OVERRIDE;
  virtual double duration() OVERRIDE;
  virtual void setDuration(double duration) OVERRIDE;
  virtual void endOfStream(EndOfStreamStatus status) OVERRIDE;

 private:
  media::ChunkDemuxer* demuxer_;  // Owned by WebMediaPlayerImpl.
  media::LogCB log_cb_;

  DISALLOW_COPY_AND_ASSIGN(WebMediaSourceClientImpl);
};

}  // namespace webkit_media

#endif  // WEBKIT_MEDIA_WEBMEDIASOURCECLIENT_IMPL_H_
