// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebSolidColorLayerImpl_h
#define WebSolidColorLayerImpl_h

#include "base/memory/scoped_ptr.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebSolidColorLayer.h"
#include "webkit/compositor_bindings/webkit_compositor_bindings_export.h"

namespace WebKit {
class WebLayerImpl;

class WebSolidColorLayerImpl : public WebSolidColorLayer {
 public:
  WEBKIT_COMPOSITOR_BINDINGS_EXPORT WebSolidColorLayerImpl();
  virtual ~WebSolidColorLayerImpl();

  // WebSolidColorLayer implementation.
  virtual WebLayer* layer() OVERRIDE;
  virtual void setBackgroundColor(WebColor) OVERRIDE;

 private:
  scoped_ptr<WebLayerImpl> layer_;
};

}  // namespace WebKit

#endif  // WebSolidColorLayerImpl_h

