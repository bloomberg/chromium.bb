// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_COMPOSITOR_BINDINGS_WEB_VIDEO_LAYER_IMPL_H_
#define WEBKIT_COMPOSITOR_BINDINGS_WEB_VIDEO_LAYER_IMPL_H_

#include "base/memory/scoped_ptr.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebVideoLayer.h"
#include "webkit/compositor_bindings/webkit_compositor_bindings_export.h"

namespace WebKit { class WebVideoFrameProvider; }

namespace webkit {

class WebLayerImpl;
class WebToCCVideoFrameProvider;

class WebVideoLayerImpl : public WebKit::WebVideoLayer {
 public:
  WEBKIT_COMPOSITOR_BINDINGS_EXPORT explicit WebVideoLayerImpl(
      WebKit::WebVideoFrameProvider*);
  virtual ~WebVideoLayerImpl();

  // WebKit::WebVideoLayer implementation.
  virtual WebKit::WebLayer* layer();
  virtual bool active() const;

 private:
  scoped_ptr<WebToCCVideoFrameProvider> provider_adapter_;
  scoped_ptr<WebLayerImpl> layer_;

  DISALLOW_COPY_AND_ASSIGN(WebVideoLayerImpl);
};

}

#endif  // WEBKIT_COMPOSITOR_BINDINGS_WEB_VIDEO_LAYER_IMPL_H_
