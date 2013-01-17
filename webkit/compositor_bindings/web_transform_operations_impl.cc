// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/compositor_bindings/web_transform_operations_impl.h"

namespace webkit {

#if WEB_TRANSFORM_OPERATIONS_IS_VIRTUAL
WebTransformOperationsImpl::WebTransformOperationsImpl() {
}

const cc::TransformOperations&
WebTransformOperationsImpl::AsTransformOperations() const {
  return transform_operations_;
}

bool WebTransformOperationsImpl::canBlendWith(
    const WebKit::WebTransformOperations& other) const {
  const WebTransformOperationsImpl& other_impl =
    static_cast<const WebTransformOperationsImpl&>(other);
  return transform_operations_.CanBlendWith(other_impl.transform_operations_);
}

void WebTransformOperationsImpl::appendTranslate(double x, double y, double z) {
  transform_operations_.AppendTranslate(x, y, z);
}

void WebTransformOperationsImpl::appendRotate(
    double x, double y, double z, double degrees) {
  transform_operations_.AppendRotate(x, y, z, degrees);
}

void WebTransformOperationsImpl::appendScale(double x, double y, double z) {
  transform_operations_.AppendScale(x, y, z);
}

void WebTransformOperationsImpl::appendSkew(double x, double y) {
  transform_operations_.AppendSkew(x, y);
}

void WebTransformOperationsImpl::appendPerspective(double depth) {
  transform_operations_.AppendPerspective(depth);
}

void WebTransformOperationsImpl::appendMatrix(
    const WebKit::WebTransformationMatrix& matrix) {
  transform_operations_.AppendMatrix(matrix);
}

void WebTransformOperationsImpl::appendIdentity() {
  transform_operations_.AppendIdentity();
}

bool WebTransformOperationsImpl::isIdentity() const {
  return transform_operations_.IsIdentity();
}

WebTransformOperationsImpl::~WebTransformOperationsImpl() {
}
#endif  // WEB_TRANSFORM_OPERATIONS_IS_VIRTUAL

}  // namespace webkit
