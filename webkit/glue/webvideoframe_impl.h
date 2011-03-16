// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBVIDEOFRAME_IMPL_H_
#define WEBKIT_GLUE_WEBVIDEOFRAME_IMPL_H_

#include "media/base/video_frame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebVideoFrame.h"

namespace webkit_glue {

class WebVideoFrameImpl : public WebKit::WebVideoFrame {
 public:
  // This converts a WebKit::WebVideoFrame to a media::VideoFrame.
  static media::VideoFrame* toVideoFrame(
      WebKit::WebVideoFrame* web_video_frame);

  WebVideoFrameImpl(scoped_refptr<media::VideoFrame> video_frame);
  virtual ~WebVideoFrameImpl();
  virtual WebVideoFrame::SurfaceType surfaceType() const;
  virtual WebVideoFrame::Format format() const;
  virtual unsigned width() const;
  virtual unsigned height() const;
  virtual unsigned planes() const;
  virtual int stride(unsigned plane) const;
  virtual const void* data(unsigned plane) const;
  virtual unsigned texture(unsigned plane) const;

 private:
  scoped_refptr<media::VideoFrame> video_frame_;
  DISALLOW_COPY_AND_ASSIGN(WebVideoFrameImpl);
};

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_WEBVIDEOFRAME_IMPL_H_
