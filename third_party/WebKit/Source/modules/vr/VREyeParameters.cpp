// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/vr/VREyeParameters.h"

namespace blink {

VREyeParameters::VREyeParameters() {
  offset_ = DOMFloat32Array::Create(3);
  field_of_view_ = new VRFieldOfView();
  render_width_ = 0;
  render_height_ = 0;
}

void VREyeParameters::Update(
    const device::mojom::blink::VREyeParametersPtr& eye_parameters) {
  offset_->Data()[0] = eye_parameters->offset[0];
  offset_->Data()[1] = eye_parameters->offset[1];
  offset_->Data()[2] = eye_parameters->offset[2];

  field_of_view_->SetUpDegrees(eye_parameters->fieldOfView->upDegrees);
  field_of_view_->SetDownDegrees(eye_parameters->fieldOfView->downDegrees);
  field_of_view_->SetLeftDegrees(eye_parameters->fieldOfView->leftDegrees);
  field_of_view_->SetRightDegrees(eye_parameters->fieldOfView->rightDegrees);

  render_width_ = eye_parameters->renderWidth;
  render_height_ = eye_parameters->renderHeight;
}

void VREyeParameters::Trace(blink::Visitor* visitor) {
  visitor->Trace(offset_);
  visitor->Trace(field_of_view_);
}

}  // namespace blink
