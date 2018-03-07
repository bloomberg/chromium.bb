// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef XRUtils_h
#define XRUtils_h

#include "core/typed_arrays/DOMTypedArray.h"
#include "platform/transforms/TransformationMatrix.h"

namespace blink {

DOMFloat32Array* transformationMatrixToFloat32Array(
    const TransformationMatrix&);

}  // namespace blink

#endif  // XRUtils_h
