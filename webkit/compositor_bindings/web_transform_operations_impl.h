// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_COMPOSITOR_BINDINGS_WEB_TRANSFORM_OPERATIONS_IMPL_H_
#define WEBKIT_COMPOSITOR_BINDINGS_WEB_TRANSFORM_OPERATIONS_IMPL_H_

#include "base/memory/scoped_ptr.h"
#include "cc/transform_operations.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebTransformOperations.h"
#include "webkit/compositor_bindings/webkit_compositor_bindings_export.h"

namespace webkit {

class WebTransformOperationsImpl : public WebKit::WebTransformOperations {
 public:
  WEBKIT_COMPOSITOR_BINDINGS_EXPORT WebTransformOperationsImpl();
  virtual ~WebTransformOperationsImpl();

  const cc::TransformOperations& AsTransformOperations() const;

  // Implementation of WebKit::WebTransformOperations methods
  virtual bool canBlendWith(const WebKit::WebTransformOperations& other) const
      OVERRIDE;
  virtual void appendTranslate(double x, double y, double z) OVERRIDE;
  virtual void appendRotate(double x, double y, double z, double degrees)
      OVERRIDE;
  virtual void appendScale(double x, double y, double z) OVERRIDE;
  virtual void appendSkew(double x, double y) OVERRIDE;
  virtual void appendPerspective(double depth) OVERRIDE;
  virtual void appendMatrix(const WebKit::WebTransformationMatrix&) OVERRIDE;
  virtual void appendIdentity() OVERRIDE;
  virtual bool isIdentity() const OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebTransformOperationsImpl);

  cc::TransformOperations transform_operations_;
};

}  // namespace webkit

#endif  // WEBKIT_COMPOSITOR_BINDINGS_WEB_TRANSFORM_OPERATIONS_IMPL_H_
