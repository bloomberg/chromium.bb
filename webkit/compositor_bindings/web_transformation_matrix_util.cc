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

WebTransformationMatrix WebTransformationMatrixUtil::ToWebTransformationMatrix(
    const gfx::Transform& transform) {
  return WebTransformationMatrix(transform.matrix().getDouble(0, 0),
                                 transform.matrix().getDouble(1, 0),
                                 transform.matrix().getDouble(2, 0),
                                 transform.matrix().getDouble(3, 0),
                                 transform.matrix().getDouble(0, 1),
                                 transform.matrix().getDouble(1, 1),
                                 transform.matrix().getDouble(2, 1),
                                 transform.matrix().getDouble(3, 1),
                                 transform.matrix().getDouble(0, 2),
                                 transform.matrix().getDouble(1, 2),
                                 transform.matrix().getDouble(2, 2),
                                 transform.matrix().getDouble(3, 2),
                                 transform.matrix().getDouble(0, 3),
                                 transform.matrix().getDouble(1, 3),
                                 transform.matrix().getDouble(2, 3),
                                 transform.matrix().getDouble(3, 3));

}

}  // namespace webkit
