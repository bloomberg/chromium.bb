// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebVideoLayerImpl_h
#define WebVideoLayerImpl_h

#include "base/memory/scoped_ptr.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebVideoLayer.h"
#include "webkit/compositor_bindings/webkit_compositor_bindings_export.h"

namespace webkit { class WebToCCVideoFrameProvider; }

namespace WebKit {
class WebLayerImpl;

class WebVideoLayerImpl : public WebVideoLayer {
 public:
  WEBKIT_COMPOSITOR_BINDINGS_EXPORT explicit WebVideoLayerImpl(
      WebVideoFrameProvider*);
  virtual ~WebVideoLayerImpl();

  // WebVideoLayer implementation.
  virtual WebLayer* layer() OVERRIDE;
  virtual bool active() const OVERRIDE;

 private:
  scoped_ptr<webkit::WebToCCVideoFrameProvider> provider_adapter_;
  scoped_ptr<WebLayerImpl> layer_;
};

}

#endif  // WebVideoLayerImpl_h
