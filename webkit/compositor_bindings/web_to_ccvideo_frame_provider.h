// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_COMPOSITOR_BINDINGS_WEB_TO_CCVIDEO_FRAME_PROVIDER_H_
#define WEBKIT_COMPOSITOR_BINDINGS_WEB_TO_CCVIDEO_FRAME_PROVIDER_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "cc/video_frame_provider.h"

namespace WebKit {
class WebVideoFrame;
class WebVideoFrameProvider;
}

namespace webkit {

class WebToCCVideoFrameProvider : public cc::VideoFrameProvider {
 public:
  static scoped_ptr<WebToCCVideoFrameProvider> Create(
      WebKit::WebVideoFrameProvider* web_provider);
  virtual ~WebToCCVideoFrameProvider();

  // cc::VideoFrameProvider implementation.
  virtual void SetVideoFrameProviderClient(Client*) OVERRIDE;
  virtual scoped_refptr<media::VideoFrame> GetCurrentFrame() OVERRIDE;
  virtual void PutCurrentFrame(const scoped_refptr<media::VideoFrame>& frame)
      OVERRIDE;

 private:
  explicit WebToCCVideoFrameProvider(WebKit::WebVideoFrameProvider*);

  class ClientAdapter;
  scoped_ptr<ClientAdapter> client_adapter_;
  WebKit::WebVideoFrameProvider* web_provider_;
  WebKit::WebVideoFrame* web_frame_;
};

}  // namespace webkit

#endif // WEBKIT_COMPOSITOR_BINDINGS_WEB_TO_CCVIDEO_FRAME_PROVIDER_H_
