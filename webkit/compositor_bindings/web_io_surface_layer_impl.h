// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebIOSurfaceLayerImpl_h
#define WebIOSurfaceLayerImpl_h

#include "base/memory/scoped_ptr.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebIOSurfaceLayer.h"
#include "webkit/compositor_bindings/webkit_compositor_bindings_export.h"

namespace WebKit {

class WebIOSurfaceLayerImpl : public WebIOSurfaceLayer {
 public:
  WEBKIT_COMPOSITOR_BINDINGS_EXPORT WebIOSurfaceLayerImpl();
  virtual ~WebIOSurfaceLayerImpl();

  // WebIOSurfaceLayer implementation.
  virtual WebLayer* layer() OVERRIDE;
  virtual void setIOSurfaceProperties(unsigned io_surface_id, WebSize) OVERRIDE;

 private:
  scoped_ptr<WebLayerImpl> layer_;
};

}

#endif  // WebIOSurfaceLayerImpl_h

