// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_COMPOSITOR_BINDINGS_WEB_TRANSFORMATION_MATRIX_UTIL_H_
#define WEBKIT_COMPOSITOR_BINDINGS_WEB_TRANSFORMATION_MATRIX_UTIL_H_

#include "third_party/WebKit/Source/Platform/chromium/public/WebTransformationMatrix.h"
#include "ui/gfx/transform.h"

namespace webkit {

class WebTransformationMatrixUtil {
 public:
  static gfx::Transform ToTransform(
      const WebKit::WebTransformationMatrix& matrix);

  static WebKit::WebTransformationMatrix ToWebTransformationMatrix(
      const gfx::Transform& transform);
};

}  // namespace webkit

#endif  // WEBKIT_COMPOSITOR_BINDINGS_WEB_TRANSFORMATION_MATRIX_UTIL_H_
