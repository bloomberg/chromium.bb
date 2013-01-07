// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_MEDIA_WEBVIDEOFRAME_IMPL_H_
#define WEBKIT_MEDIA_WEBVIDEOFRAME_IMPL_H_

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebVideoFrame.h"

namespace media {
class VideoFrame;
}

namespace webkit_media {

class WebVideoFrameImpl : public WebKit::WebVideoFrame {
 public:
  WebVideoFrameImpl(scoped_refptr<media::VideoFrame> video_frame);
  virtual ~WebVideoFrameImpl();

  scoped_refptr<media::VideoFrame> video_frame;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebVideoFrameImpl);
};

}  // namespace webkit_media

#endif  // WEBKIT_MEDIA_WEBVIDEOFRAME_IMPL_H_
