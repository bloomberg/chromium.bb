// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/compositor_bindings/web_to_ccvideo_frame_provider.h"

#include "base/logging.h"
#include "media/base/video_frame.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebVideoFrameProvider.h"
#include "webkit/media/webvideoframe_impl.h"

using WebKit::WebVideoFrameProvider;
using webkit_media::WebVideoFrameImpl;

namespace webkit {

scoped_ptr<WebToCCVideoFrameProvider> WebToCCVideoFrameProvider::Create(
    WebVideoFrameProvider* web_provider) {
  return make_scoped_ptr(new WebToCCVideoFrameProvider(web_provider));
}

WebToCCVideoFrameProvider::WebToCCVideoFrameProvider(
    WebVideoFrameProvider* web_provider)
    : web_provider_(web_provider), web_frame_(NULL) {}

WebToCCVideoFrameProvider::~WebToCCVideoFrameProvider() {}

class WebToCCVideoFrameProvider::ClientAdapter :
    public WebVideoFrameProvider::Client {
 public:
  explicit ClientAdapter(cc::VideoFrameProvider::Client* cc_client)
      : cc_client_(cc_client) {}
  virtual ~ClientAdapter() {}

  // WebVideoFrameProvider::Client implementation.
  virtual void stopUsingProvider() { cc_client_->StopUsingProvider(); }

  virtual void didReceiveFrame() { cc_client_->DidReceiveFrame(); }

  virtual void didUpdateMatrix(const float* matrix) {
    cc_client_->DidUpdateMatrix(matrix);
  }

 private:
  cc::VideoFrameProvider::Client* cc_client_;
};

void WebToCCVideoFrameProvider::SetVideoFrameProviderClient(Client* client) {
  scoped_ptr<ClientAdapter> client_adapter;
  if (client)
    client_adapter.reset(new ClientAdapter(client));
  web_provider_->setVideoFrameProviderClient(client_adapter.get());
  client_adapter_ = client_adapter.Pass();
}

scoped_refptr<media::VideoFrame> WebToCCVideoFrameProvider::GetCurrentFrame() {
  web_frame_ = web_provider_->getCurrentFrame();
  if (!web_frame_)
    return scoped_refptr<media::VideoFrame>();
  WebVideoFrameImpl* impl = static_cast<WebVideoFrameImpl*>(web_frame_);
  return impl->video_frame;
}

void WebToCCVideoFrameProvider::PutCurrentFrame(
    const scoped_refptr<media::VideoFrame>& frame) {
  if (!frame) {
    DCHECK(!web_frame_);
    web_provider_->putCurrentFrame(web_frame_);
    return;
  }
  DCHECK(web_frame_);
  WebVideoFrameImpl* impl = static_cast<WebVideoFrameImpl*>(web_frame_);
  DCHECK_EQ(impl->video_frame.get(), frame.get());
  web_provider_->putCurrentFrame(web_frame_);
  web_frame_ = NULL;
}

}  // namespace webkit
