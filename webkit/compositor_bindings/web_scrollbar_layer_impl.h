// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebScrollbarLayerImpl_h
#define WebScrollbarLayerImpl_h

#include "base/memory/scoped_ptr.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebScrollbarLayer.h"
#include "webkit/compositor_bindings/webkit_compositor_bindings_export.h"

namespace WebKit {
class WebLayerImpl;

class WebScrollbarLayerImpl : public WebScrollbarLayer {
 public:
  WEBKIT_COMPOSITOR_BINDINGS_EXPORT WebScrollbarLayerImpl(
      WebScrollbar*,
      WebScrollbarThemePainter,
      WebScrollbarThemeGeometry*);
  virtual ~WebScrollbarLayerImpl();

  // WebScrollbarLayer implementation.
  virtual WebLayer* layer() OVERRIDE;
  virtual void setScrollLayer(WebLayer*) OVERRIDE;

 private:
  scoped_ptr<WebLayerImpl> layer_;
};

}

#endif  // WebScrollbarLayerImpl_h
