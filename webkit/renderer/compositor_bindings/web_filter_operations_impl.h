// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_RENDERER_COMPOSITOR_BINDINGS_WEB_FILTER_OPERATIONS_IMPL_H_
#define WEBKIT_RENDERER_COMPOSITOR_BINDINGS_WEB_FILTER_OPERATIONS_IMPL_H_

#include "cc/output/filter_operations.h"
#include "third_party/WebKit/public/platform/WebFilterOperations.h"
#include "webkit/renderer/compositor_bindings/webkit_compositor_bindings_export.h"

namespace webkit {

class WebFilterOperationsImpl : public WebKit::WebFilterOperations {
 public:
  WEBKIT_COMPOSITOR_BINDINGS_EXPORT WebFilterOperationsImpl();
  virtual ~WebFilterOperationsImpl();

  const cc::FilterOperations& AsFilterOperations() const;

  // Implementation of WebKit::WebFilterOperations methods
  virtual void appendGrayscaleFilter(float amount);
  virtual void appendSepiaFilter(float amount);
  virtual void appendSaturateFilter(float amount);
  virtual void appendHueRotateFilter(float amount);
  virtual void appendInvertFilter(float amount);
  virtual void appendBrightnessFilter(float amount);
  virtual void appendContrastFilter(float amount);
  virtual void appendOpacityFilter(float amount);
  virtual void appendBlurFilter(float amount);
  virtual void appendDropShadowFilter(WebKit::WebPoint offset,
                                      float std_deviation,
                                      WebKit::WebColor color);
  virtual void appendColorMatrixFilter(SkScalar matrix[20]);
  virtual void appendZoomFilter(float amount, int inset);
  virtual void appendSaturatingBrightnessFilter(float amount);

  virtual void clear();

 private:
  cc::FilterOperations filter_operations_;

  DISALLOW_COPY_AND_ASSIGN(WebFilterOperationsImpl);
};

}  // namespace webkit

#endif  // WEBKIT_RENDERER_COMPOSITOR_BINDINGS_WEB_FILTER_OPERATIONS_IMPL_H_
