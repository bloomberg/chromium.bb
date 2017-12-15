// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/xr/XRView.h"

#include "modules/xr/XRLayer.h"
#include "modules/xr/XRPresentationFrame.h"
#include "modules/xr/XRSession.h"
#include "modules/xr/XRViewport.h"
#include "platform/geometry/FloatPoint3D.h"

namespace blink {

XRView::XRView(XRSession* session, Eye eye)
    : eye_(eye),
      session_(session),
      projection_matrix_(DOMFloat32Array::Create(16)) {
  eye_string_ = (eye_ == kEyeLeft ? "left" : "right");
}

XRSession* XRView::session() const {
  return session_;
}

XRViewport* XRView::getViewport(XRLayer* layer) const {
  if (!layer || layer->session() != session_)
    return nullptr;

  return layer->GetViewport(eye_);
}

void XRView::UpdateProjectionMatrixFromFoV(float up_rad,
                                           float down_rad,
                                           float left_rad,
                                           float right_rad,
                                           float near_depth,
                                           float far_depth) {
  float up_tan = tanf(up_rad);
  float down_tan = tanf(down_rad);
  float left_tan = tanf(left_rad);
  float right_tan = tanf(right_rad);
  float x_scale = 2.0f / (left_tan + right_tan);
  float y_scale = 2.0f / (up_tan + down_tan);
  float inv_nf = 1.0f / (near_depth - far_depth);

  float* out = projection_matrix_->Data();
  out[0] = x_scale;
  out[1] = 0.0f;
  out[2] = 0.0f;
  out[3] = 0.0f;
  out[4] = 0.0f;
  out[5] = y_scale;
  out[6] = 0.0f;
  out[7] = 0.0f;
  out[8] = -((left_tan - right_tan) * x_scale * 0.5);
  out[9] = ((up_tan - down_tan) * y_scale * 0.5);
  out[10] = (near_depth + far_depth) * inv_nf;
  out[11] = -1.0f;
  out[12] = 0.0f;
  out[13] = 0.0f;
  out[14] = (2.0f * far_depth * near_depth) * inv_nf;
  out[15] = 0.0f;
}

void XRView::UpdateOffset(float x, float y, float z) {
  offset_.Set(x, y, z);
}

void XRView::Trace(blink::Visitor* visitor) {
  visitor->Trace(session_);
  visitor->Trace(projection_matrix_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
