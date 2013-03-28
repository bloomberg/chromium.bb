// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_MEDIA_WEBSOURCEBUFFER_IMPL_H_
#define WEBKIT_MEDIA_WEBSOURCEBUFFER_IMPL_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSourceBuffer.h"

namespace media {
class ChunkDemuxer;
}

namespace webkit_media {

class WebSourceBufferImpl : public WebKit::WebSourceBuffer {
 public:
  WebSourceBufferImpl(const std::string& id,
                      scoped_refptr<media::ChunkDemuxer> demuxer);
  virtual ~WebSourceBufferImpl();

  // WebKit::WebSourceBuffer implementation.
  virtual WebKit::WebTimeRanges buffered() OVERRIDE;
  virtual void append(const unsigned char* data, unsigned length) OVERRIDE;
  virtual void abort() OVERRIDE;
  virtual bool setTimestampOffset(double offset) OVERRIDE;
  virtual void removedFromMediaSource() OVERRIDE;

 private:
  std::string id_;
  scoped_refptr<media::ChunkDemuxer> demuxer_;

  DISALLOW_COPY_AND_ASSIGN(WebSourceBufferImpl);
};

}  // namespace webkit_media

#endif  // WEBKIT_MEDIA_WEBSOURCEBUFFER_IMPL_H_
