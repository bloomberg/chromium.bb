// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_MEDIA_WEBVIDEOFRAME_IMPL_H_
#define WEBKIT_MEDIA_WEBVIDEOFRAME_IMPL_H_

#include "base/compiler_specific.h"
#include "media/base/video_frame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebVideoFrame.h"

namespace webkit_media {

class WebVideoFrameImpl : public WebKit::WebVideoFrame {
 public:
  // This converts a WebKit::WebVideoFrame to a media::VideoFrame.
  static media::VideoFrame* toVideoFrame(
      WebKit::WebVideoFrame* web_video_frame);

  WebVideoFrameImpl(scoped_refptr<media::VideoFrame> video_frame);
  virtual ~WebVideoFrameImpl();
  virtual WebVideoFrame::Format format() const;
  virtual unsigned width() const;
  virtual unsigned height() const;
  virtual unsigned planes() const;
  virtual int stride(unsigned plane) const;
  virtual const void* data(unsigned plane) const;
  virtual unsigned textureId() const;
  virtual unsigned textureTarget() const;

 private:
  scoped_refptr<media::VideoFrame> video_frame_;
  DISALLOW_COPY_AND_ASSIGN(WebVideoFrameImpl);
};

}  // namespace webkit_media

#endif  // WEBKIT_MEDIA_WEBVIDEOFRAME_IMPL_H_
