// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_plane.h"
#include "third_party/blink/renderer/modules/xr/type_converters.h"
#include "third_party/blink/renderer/modules/xr/xr_pose.h"
#include "third_party/blink/renderer/modules/xr/xr_reference_space.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"

namespace blink {

XRPlane::XRPlane(XRSession* session,
                 const device::mojom::blink::XRPlaneDataPtr& plane_data)
    : XRPlane(session,
              mojo::ConvertTo<base::Optional<blink::XRPlane::Orientation>>(
                  plane_data->orientation),
              mojo::ConvertTo<blink::TransformationMatrix>(plane_data->pose),
              mojo::ConvertTo<HeapVector<Member<DOMPointReadOnly>>>(
                  plane_data->polygon)) {}

XRPlane::XRPlane(XRSession* session,
                 const base::Optional<Orientation>& orientation,
                 const TransformationMatrix& pose_matrix,
                 const HeapVector<Member<DOMPointReadOnly>>& polygon)
    : polygon_(polygon),
      orientation_(orientation),
      pose_matrix_(pose_matrix),
      session_(session) {
  DVLOG(3) << __func__;
}

XRPose* XRPlane::getPose(XRReferenceSpace* reference_space) const {
  return MakeGarbageCollected<XRPose>(
      reference_space->GetViewerPoseMatrix(
          std::make_unique<TransformationMatrix>(pose_matrix_)),
      session_->EmulatedPosition());
}

String XRPlane::orientation() const {
  if (orientation_.has_value()) {
    switch (orientation_.value()) {
      case Orientation::kHorizontal:
        return "Horizontal";
      case Orientation::kVertical:
        return "Vertical";
    }
  }
  return "";
}

HeapVector<Member<DOMPointReadOnly>> XRPlane::polygon() const {
  // Returns copy of a vector - by design. This way, JavaScript code could
  // store the state of the plane's polygon in frame N just by storing the
  // array (`let polygon = plane.polygon`) - the stored array won't be affected
  // by the changes to the plane that could happen in frames >N.
  return polygon_;
}

void XRPlane::Update(const device::mojom::blink::XRPlaneDataPtr& plane_data) {
  DVLOG(3) << __func__;

  orientation_ = mojo::ConvertTo<base::Optional<blink::XRPlane::Orientation>>(
      plane_data->orientation);
  pose_matrix_ = mojo::ConvertTo<blink::TransformationMatrix>(plane_data->pose);
  polygon_ = mojo::ConvertTo<HeapVector<Member<DOMPointReadOnly>>>(
      plane_data->polygon);
}

void XRPlane::Trace(blink::Visitor* visitor) {
  visitor->Trace(polygon_);
  visitor->Trace(session_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
