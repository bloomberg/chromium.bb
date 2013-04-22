// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_MEDIA_WEBSOURCEBUFFER_IMPL_H_
#define WEBKIT_MEDIA_WEBSOURCEBUFFER_IMPL_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSourceBuffer.h"

namespace media {
class ChunkDemuxer;
}

namespace webkit_media {

class WebSourceBufferImpl : public WebKit::WebSourceBuffer {
 public:
  WebSourceBufferImpl(const std::string& id, media::ChunkDemuxer* demuxer);
  virtual ~WebSourceBufferImpl();

  // WebKit::WebSourceBuffer implementation.
  virtual WebKit::WebTimeRanges buffered() OVERRIDE;
  virtual void append(const unsigned char* data, unsigned length) OVERRIDE;
  virtual void abort() OVERRIDE;
  virtual bool setTimestampOffset(double offset) OVERRIDE;
  virtual void removedFromMediaSource() OVERRIDE;

 private:
  std::string id_;
  media::ChunkDemuxer* demuxer_;  // Owned by WebMediaPlayerImpl.

  DISALLOW_COPY_AND_ASSIGN(WebSourceBufferImpl);
};

}  // namespace webkit_media

#endif  // WEBKIT_MEDIA_WEBSOURCEBUFFER_IMPL_H_
