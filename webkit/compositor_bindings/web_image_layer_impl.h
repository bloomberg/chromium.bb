// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebImageLayerImpl_h
#define WebImageLayerImpl_h

#include "base/memory/scoped_ptr.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebImageLayer.h"
#include "webkit/compositor_bindings/webkit_compositor_bindings_export.h"

namespace WebKit {
class WebLayerImpl;

class WebImageLayerImpl : public WebImageLayer {
 public:
  WEBKIT_COMPOSITOR_BINDINGS_EXPORT WebImageLayerImpl();
  virtual ~WebImageLayerImpl();

  // WebImageLayer implementation.
  virtual WebLayer* layer() OVERRIDE;
  virtual void setBitmap(SkBitmap) OVERRIDE;

 private:
  scoped_ptr<WebLayerImpl> layer_;
};

}

#endif  // WebImageLayerImpl_h
