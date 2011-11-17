// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
  virtual WebVideoFrame::Format format() const OVERRIDE;
  virtual unsigned width() const OVERRIDE;
  virtual unsigned height() const OVERRIDE;
  virtual unsigned planes() const OVERRIDE;
  virtual int stride(unsigned plane) const OVERRIDE;
  virtual const void* data(unsigned plane) const OVERRIDE;

 private:
  scoped_refptr<media::VideoFrame> video_frame_;
  DISALLOW_COPY_AND_ASSIGN(WebVideoFrameImpl);
};

}  // namespace webkit_media

#endif  // WEBKIT_MEDIA_WEBVIDEOFRAME_IMPL_H_
