// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_MEDIA_MEDIA_STREAM_CLIENT_H_
#define WEBKIT_GLUE_MEDIA_MEDIA_STREAM_CLIENT_H_

#include "base/memory/ref_counted.h"

class GURL;

namespace media {
class VideoDecoder;
class MessageLoopFactory;
}

namespace webkit_glue {

// Define an interface for media stream client to get some information about
// the media stream.
class MediaStreamClient {
 public:
  virtual scoped_refptr<media::VideoDecoder> GetVideoDecoder(
      const GURL& url,
      media::MessageLoopFactory* message_loop_factory) = 0;

 protected:
  virtual ~MediaStreamClient() {}
};

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_MEDIA_MEDIA_STREAM_CLIENT_H_
