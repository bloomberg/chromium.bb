// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_UTILS_H_

#include "device/vr/public/mojom/pose.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/modules/xr/xr_webgl_rendering_context.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class DOMPointReadOnly;
class TransformationMatrix;
class WebGLRenderingContextBase;

DOMFloat32Array* transformationMatrixToDOMFloat32Array(
    const TransformationMatrix&);

TransformationMatrix DOMFloat32ArrayToTransformationMatrix(DOMFloat32Array*);

TransformationMatrix WTFFloatVectorToTransformationMatrix(const Vector<float>&);

DOMPointReadOnly* makeNormalizedQuaternion(double x,
                                           double y,
                                           double z,
                                           double w);

WebGLRenderingContextBase* webglRenderingContextBaseFromUnion(
    const XRWebGLRenderingContext&);

constexpr char kUnableToNormalizeZeroLength[] =
    "Unable to normalize vector of length 0.";

// Conversion method from transformation matrix to device::Pose. The conversion
// may fail if the matrix cannot be decomposed. In case of failure, the method
// will return base::nullopt.
base::Optional<device::Pose> CreatePose(
    const blink::TransformationMatrix& matrix);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_UTILS_H_
