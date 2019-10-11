// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/vr/vr_stage_parameters.h"
#include "device/vr/public/mojom/vr_service.mojom-blink.h"
#include "third_party/blink/renderer/modules/xr/xr_utils.h"
#include "third_party/blink/renderer/platform/transforms/transformation_matrix.h"

namespace blink {

VRStageParameters::VRStageParameters() : size_x_(0.0f), size_z_(0.0f) {
  // Set the sitting to standing transform to identity matrix
  standing_transform_ = DOMFloat32Array::Create(16);
  standing_transform_->Data()[0] = 1.0f;
  standing_transform_->Data()[5] = 1.0f;
  standing_transform_->Data()[10] = 1.0f;
  standing_transform_->Data()[15] = 1.0f;
}

void VRStageParameters::Update(
    const device::mojom::blink::VRStageParametersPtr& stage) {
  standing_transform_ = transformationMatrixToDOMFloat32Array(
      TransformationMatrix(stage->standing_transform.matrix()));
  size_x_ = stage->size_x;
  size_z_ = stage->size_z;
}

void VRStageParameters::Trace(blink::Visitor* visitor) {
  visitor->Trace(standing_transform_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
