// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_RENDERER_COMPOSITOR_BINDINGS_WEB_TRANSFORM_OPERATIONS_IMPL_H_
#define WEBKIT_RENDERER_COMPOSITOR_BINDINGS_WEB_TRANSFORM_OPERATIONS_IMPL_H_

#include "base/memory/scoped_ptr.h"
#include "cc/animation/transform_operations.h"
#include "third_party/WebKit/public/platform/WebTransformOperations.h"
#include "webkit/renderer/compositor_bindings/webkit_compositor_bindings_export.h"

namespace webkit {

class WebTransformOperationsImpl : public WebKit::WebTransformOperations {
 public:
  WEBKIT_COMPOSITOR_BINDINGS_EXPORT WebTransformOperationsImpl();
  virtual ~WebTransformOperationsImpl();

  const cc::TransformOperations& AsTransformOperations() const;

  // Implementation of WebKit::WebTransformOperations methods
  virtual bool canBlendWith(const WebKit::WebTransformOperations& other) const;
  virtual void appendTranslate(double x, double y, double z);
  virtual void appendRotate(double x, double y, double z, double degrees);
  virtual void appendScale(double x, double y, double z);
  virtual void appendSkew(double x, double y);
  virtual void appendPerspective(double depth);
  virtual void appendMatrix(const SkMatrix44&);
  virtual void appendIdentity();
  virtual bool isIdentity() const;

 private:
  cc::TransformOperations transform_operations_;

  DISALLOW_COPY_AND_ASSIGN(WebTransformOperationsImpl);
};

}  // namespace webkit

#endif  // WEBKIT_RENDERER_COMPOSITOR_BINDINGS_WEB_TRANSFORM_OPERATIONS_IMPL_H_
