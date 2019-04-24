// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_frame.h"

#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/xr/xr_input_pose.h"
#include "third_party/blink/renderer/modules/xr/xr_input_source.h"
#include "third_party/blink/renderer/modules/xr/xr_reference_space.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"
#include "third_party/blink/renderer/modules/xr/xr_view.h"
#include "third_party/blink/renderer/modules/xr/xr_viewer_pose.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

namespace {

const char kInactiveFrame[] =
    "XRFrame access outside the callback that produced it is invalid.";

const char kSessionMismatch[] = "XRSpace and XRFrame sessions do not match.";

}  // namespace

XRFrame::XRFrame(XRSession* session) : session_(session) {}

XRViewerPose* XRFrame::getViewerPose(XRReferenceSpace* reference_space,
                                     ExceptionState& exception_state) const {
  if (!active_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kInactiveFrame);
    return nullptr;
  }

  session_->LogGetPose();

  // Must use a reference space created from the same session.
  if (!reference_space || reference_space->session() != session_) {
    return nullptr;
  }

  std::unique_ptr<TransformationMatrix> pose;

  // If we don't have a valid base pose request the reference space's default
  // pose. Most common when tracking is lost.
  if (base_pose_matrix_) {
    pose = reference_space->TransformBasePose(*base_pose_matrix_);
  } else {
    pose = reference_space->DefaultPose();
  }

  if (!pose) {
    return nullptr;
  }

  // Can only update an XRViewerPose's views with an invertible matrix.
  if (!pose->IsInvertible()) {
    return nullptr;
  }

  return MakeGarbageCollected<XRViewerPose>(session(), std::move(pose));
}

XRInputPose* XRFrame::getInputPose(XRInputSource* input_source,
                                   XRReferenceSpace* reference_space,
                                   ExceptionState& exception_state) const {
  if (!active_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kInactiveFrame);
    return nullptr;
  }

  if (!input_source || !reference_space) {
    return nullptr;
  }

  // Must use an input source and coordinate system from the same session.
  if (input_source->session() != session_ ||
      reference_space->session() != session_) {
    return nullptr;
  }

  switch (input_source->target_ray_mode_) {
    case XRInputSource::kScreen: {
      // If the pointer origin is the screen we need the head's base pose and
      // the pointer transform matrix to continue. The pointer transform will
      // represent the point the canvas was clicked as an offset from the view.
      if (!base_pose_matrix_ || !input_source->pointer_transform_matrix_) {
        return nullptr;
      }

      // Multiply the head pose and pointer transform to get the final pointer.
      std::unique_ptr<TransformationMatrix> pointer_pose =
          reference_space->TransformBasePose(*base_pose_matrix_);
      pointer_pose->Multiply(*(input_source->pointer_transform_matrix_));

      return MakeGarbageCollected<XRInputPose>(std::move(pointer_pose),
                                               nullptr);
    }
    case XRInputSource::kGaze: {
      // If the pointer origin is the users head, this is a gaze cursor and the
      // returned pointer is based on the device pose. If we don't have a valid
      // base pose (most common when tracking is lost) return null.
      if (!base_pose_matrix_) {
        return nullptr;
      }

      // Just return the head pose as the pointer pose.
      std::unique_ptr<TransformationMatrix> pointer_pose =
          reference_space->TransformBasePose(*base_pose_matrix_);

      return MakeGarbageCollected<XRInputPose>(
          std::move(pointer_pose), nullptr, input_source->emulatedPosition());
    }
    case XRInputSource::kTrackedPointer: {
      // If the input source doesn't have a base pose return null;
      if (!input_source->base_pose_matrix_) {
        return nullptr;
      }

      std::unique_ptr<TransformationMatrix> grip_pose =
          reference_space->TransformBaseInputPose(
              *(input_source->base_pose_matrix_), *base_pose_matrix_);

      if (!grip_pose) {
        return nullptr;
      }

      std::unique_ptr<TransformationMatrix> pointer_pose(
          TransformationMatrix::Create(*grip_pose));

      if (input_source->pointer_transform_matrix_) {
        pointer_pose->Multiply(*(input_source->pointer_transform_matrix_));
      }

      return MakeGarbageCollected<XRInputPose>(
          std::move(pointer_pose), std::move(grip_pose),
          input_source->emulatedPosition());
    }
  }

  return nullptr;
}

// Return an XRPose that has a transform mapping to space A from space B, while
// accounting for the base pose matrix of this frame. If computing a transform
// isn't possible, return nullptr.
XRPose* XRFrame::getPose(XRSpace* space_A,
                         XRSpace* space_B,
                         ExceptionState& exception_state) {
  if (!active_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kInactiveFrame);
    return nullptr;
  }

  if (space_A->session() != session_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kSessionMismatch);
    return nullptr;
  }

  if (space_B->session() != session_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kSessionMismatch);
    return nullptr;
  }

  std::unique_ptr<TransformationMatrix> mojo_from_A =
      space_A->GetTransformToMojoSpace();
  if (!mojo_from_A) {
    return nullptr;
  }

  // Rigid transforms should always be invertible.
  DCHECK(mojo_from_A->IsInvertible());
  TransformationMatrix A_from_mojo = mojo_from_A->Inverse();

  std::unique_ptr<TransformationMatrix> mojo_from_B =
      space_B->GetTransformToMojoSpace();
  if (!mojo_from_B) {
    return nullptr;
  }

  // TODO(jacde): Update how EmulatedPosition is determined here once spec issue
  // https://github.com/immersive-web/webxr/issues/534 has been resolved.
  TransformationMatrix A_from_B = A_from_mojo.Multiply(*mojo_from_B);
  return MakeGarbageCollected<XRPose>(TransformationMatrix::Create(A_from_B),
                                      session_->EmulatedPosition());
}

void XRFrame::SetBasePoseMatrix(const TransformationMatrix& base_pose_matrix) {
  base_pose_matrix_ = TransformationMatrix::Create(base_pose_matrix);
}

void XRFrame::Deactivate() {
  active_ = false;
}

void XRFrame::Trace(blink::Visitor* visitor) {
  visitor->Trace(session_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
