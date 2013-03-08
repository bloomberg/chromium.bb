// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/compositor_bindings/web_transformation_matrix_util.h"

using WebKit::WebTransformationMatrix;

namespace webkit {

gfx::Transform WebTransformationMatrixUtil::ToTransform(
    const WebTransformationMatrix& matrix) {
  return gfx::Transform(matrix.m11(),
                        matrix.m21(),
                        matrix.m31(),
                        matrix.m41(),
                        matrix.m12(),
                        matrix.m22(),
                        matrix.m32(),
                        matrix.m42(),
                        matrix.m13(),
                        matrix.m23(),
                        matrix.m33(),
                        matrix.m43(),
                        matrix.m14(),
                        matrix.m24(),
                        matrix.m34(),
                        matrix.m44());
}

}  // namespace webkit
