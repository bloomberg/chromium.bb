// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_reference_space.h"

#include "device/vr/public/mojom/vr_service.mojom-blink.h"
#include "third_party/blink/renderer/modules/xr/xr_pose.h"
#include "third_party/blink/renderer/modules/xr/xr_reference_space_event.h"
#include "third_party/blink/renderer/modules/xr/xr_rigid_transform.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"
#include "third_party/blink/renderer/modules/xr/xr_utils.h"

namespace blink {

// Rough estimate of avg human eye height in meters.
const double kDefaultEmulationHeightMeters = 1.6;

XRReferenceSpace::Type XRReferenceSpace::StringToReferenceSpaceType(
    const String& reference_space_type) {
  if (reference_space_type == "viewer") {
    return XRReferenceSpace::Type::kTypeViewer;
  } else if (reference_space_type == "local") {
    return XRReferenceSpace::Type::kTypeLocal;
  } else if (reference_space_type == "local-floor") {
    return XRReferenceSpace::Type::kTypeLocalFloor;
  } else if (reference_space_type == "bounded-floor") {
    return XRReferenceSpace::Type::kTypeBoundedFloor;
  } else if (reference_space_type == "unbounded") {
    return XRReferenceSpace::Type::kTypeUnbounded;
  }
  NOTREACHED();
  return Type::kTypeViewer;
}

// origin offset starts as identity transform
XRReferenceSpace::XRReferenceSpace(XRSession* session, Type type)
    : XRReferenceSpace(session,
                       MakeGarbageCollected<XRRigidTransform>(nullptr, nullptr),
                       type) {}

XRReferenceSpace::XRReferenceSpace(XRSession* session,
                                   XRRigidTransform* origin_offset,
                                   Type type)
    : XRSpace(session), origin_offset_(origin_offset), type_(type) {}

XRReferenceSpace::~XRReferenceSpace() = default;

XRPose* XRReferenceSpace::getPose(XRSpace* other_space) {
  if (type_ == Type::kTypeViewer) {
    base::Optional<TransformationMatrix> other_offset_from_viewer =
        other_space->OffsetFromViewer();
    if (!other_offset_from_viewer) {
      return nullptr;
    }

    auto viewer_from_offset = NativeFromOffsetMatrix();

    auto other_offset_from_offset =
        *other_offset_from_viewer * viewer_from_offset;

    return MakeGarbageCollected<XRPose>(other_offset_from_offset,
                                        session()->EmulatedPosition());
  } else {
    return XRSpace::getPose(other_space);
  }
}

void XRReferenceSpace::SetFloorFromMojo() {
  const device::mojom::blink::VRDisplayInfoPtr& display_info =
      session()->GetVRDisplayInfo();

  if (display_info && display_info->stage_parameters) {
    // Use the transform given by xrDisplayInfo's stage_parameters if available.
    floor_from_mojo_ = std::make_unique<TransformationMatrix>(
        display_info->stage_parameters->standing_transform.matrix());
  } else {
    floor_from_mojo_.reset();
  }

  display_info_id_ = session()->DisplayInfoPtrId();
}

base::Optional<TransformationMatrix> XRReferenceSpace::NativeFromMojo() {
  switch (type_) {
    case Type::kTypeLocal: {
      // The session is the source of truth for latest state of the transform
      // between local space and mojo space.
      auto mojo_from_local = session()->GetMojoFrom(Type::kTypeLocal);
      if (!mojo_from_local) {
        return base::nullopt;
      }

      DCHECK(mojo_from_local->IsInvertible());
      return mojo_from_local->Inverse();
    }
    case Type::kTypeLocalFloor: {
      // Check first to see if the xrDisplayInfo has updated since the last
      // call. If so, update the floor-level transform.
      if (display_info_id_ != session()->DisplayInfoPtrId())
        SetFloorFromMojo();

      if (floor_from_mojo_) {
        return *floor_from_mojo_;
      }

      // If the floor-level transform is unavailable, try to use the default
      // transform based off of local space:
      auto mojo_from_local = session()->GetMojoFrom(Type::kTypeLocal);
      if (!mojo_from_local) {
        return base::nullopt;
      }

      DCHECK(mojo_from_local->IsInvertible());
      auto local_from_mojo = mojo_from_local->Inverse();

      // local-floor_from_local transform corresponding to the default height.
      auto floor_from_local = TransformationMatrix().Translate3d(
          0, kDefaultEmulationHeightMeters, 0);

      return floor_from_local * local_from_mojo;
    }
    case Type::kTypeViewer: {
      auto mojo_from_viewer = session()->GetMojoFrom(Type::kTypeViewer);
      // If we don't have mojo_from_viewer, then it's the default pose,
      // which is the identity pose.
      if (!mojo_from_viewer)
        return TransformationMatrix();

      // Otherwise we need to return viewer_from_mojo which is the inverse.
      DCHECK(mojo_from_viewer->IsInvertible());
      return mojo_from_viewer->Inverse();
    }
    case Type::kTypeUnbounded: {
      // The session is the source of truth for latest state of the transform
      // between unbounded space and mojo space.
      auto mojo_from_unbounded = session()->GetMojoFrom(Type::kTypeUnbounded);
      if (!mojo_from_unbounded) {
        return base::nullopt;
      }

      DCHECK(mojo_from_unbounded->IsInvertible());
      return mojo_from_unbounded->Inverse();
    }
    case Type::kTypeBoundedFloor: {
      NOTREACHED() << "kTypeBoundedFloor should be handled by subclass";
      return base::nullopt;
    }
  }
}

base::Optional<TransformationMatrix> XRReferenceSpace::NativeFromViewer(
    const base::Optional<TransformationMatrix>& mojo_from_viewer) {
  if (type_ == Type::kTypeViewer) {
    // Special case for viewer space, always return an identity matrix
    // explicitly. In theory the default behavior of multiplying NativeFromMojo
    // onto MojoFromViewer would be equivalent, but that would likely return an
    // almost-identity due to rounding errors.
    return TransformationMatrix();
  }

  if (!mojo_from_viewer)
    return base::nullopt;

  // Return native_from_viewer = native_from_mojo * mojo_from_viewer
  auto native_from_viewer = NativeFromMojo();
  if (!native_from_viewer)
    return base::nullopt;
  native_from_viewer->Multiply(*mojo_from_viewer);
  return native_from_viewer;
}

base::Optional<TransformationMatrix> XRReferenceSpace::MojoFromNative() {
  return XRSpace::TryInvert(NativeFromMojo());
}

TransformationMatrix XRReferenceSpace::NativeFromOffsetMatrix() {
  return origin_offset_->TransformMatrix();
}

TransformationMatrix XRReferenceSpace::OffsetFromNativeMatrix() {
  return origin_offset_->InverseTransformMatrix();
}

bool XRReferenceSpace::IsStationary() const {
  switch (type_) {
    case XRReferenceSpace::Type::kTypeLocal:
    case XRReferenceSpace::Type::kTypeLocalFloor:
    case XRReferenceSpace::Type::kTypeBoundedFloor:
    case XRReferenceSpace::Type::kTypeUnbounded:
      return true;
    case XRReferenceSpace::Type::kTypeViewer:
      return false;
  }
}

XRReferenceSpace::Type XRReferenceSpace::GetType() const {
  return type_;
}

XRReferenceSpace* XRReferenceSpace::getOffsetReferenceSpace(
    XRRigidTransform* additional_offset) {
  auto matrix =
      NativeFromOffsetMatrix().Multiply(additional_offset->TransformMatrix());

  auto* result_transform = MakeGarbageCollected<XRRigidTransform>(matrix);
  return cloneWithOriginOffset(result_transform);
}

XRReferenceSpace* XRReferenceSpace::cloneWithOriginOffset(
    XRRigidTransform* origin_offset) {
  return MakeGarbageCollected<XRReferenceSpace>(this->session(), origin_offset,
                                                type_);
}

base::Optional<XRNativeOriginInformation> XRReferenceSpace::NativeOrigin()
    const {
  return XRNativeOriginInformation::Create(this);
}

void XRReferenceSpace::Trace(Visitor* visitor) {
  visitor->Trace(origin_offset_);
  XRSpace::Trace(visitor);
}

void XRReferenceSpace::OnReset() {
  if (type_ != Type::kTypeViewer) {
    DispatchEvent(
        *XRReferenceSpaceEvent::Create(event_type_names::kReset, this));
  }
}

}  // namespace blink
