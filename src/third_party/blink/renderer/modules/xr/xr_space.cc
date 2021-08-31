// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_space.h"

#include "base/stl_util.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/xr/xr_input_source.h"
#include "third_party/blink/renderer/modules/xr/xr_pose.h"
#include "third_party/blink/renderer/modules/xr/xr_rigid_transform.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"

namespace blink {

XRSpace::XRSpace(XRSession* session) : session_(session) {}

XRSpace::~XRSpace() = default;

absl::optional<TransformationMatrix> XRSpace::NativeFromViewer(
    const absl::optional<TransformationMatrix>& mojo_from_viewer) {
  if (!mojo_from_viewer)
    return absl::nullopt;

  absl::optional<TransformationMatrix> native_from_mojo = NativeFromMojo();
  if (!native_from_mojo)
    return absl::nullopt;

  native_from_mojo->Multiply(*mojo_from_viewer);

  // This is now native_from_viewer
  return native_from_mojo;
}

TransformationMatrix XRSpace::NativeFromOffsetMatrix() {
  TransformationMatrix identity;
  return identity;
}

TransformationMatrix XRSpace::OffsetFromNativeMatrix() {
  TransformationMatrix identity;
  return identity;
}

absl::optional<TransformationMatrix> XRSpace::MojoFromOffsetMatrix() {
  auto maybe_mojo_from_native = MojoFromNative();
  if (!maybe_mojo_from_native) {
    return absl::nullopt;
  }

  // Modifies maybe_mojo_from_native - it becomes mojo_from_offset_matrix.
  // Saves a heap allocation since there is no need to create a new unique_ptr.
  maybe_mojo_from_native->Multiply(NativeFromOffsetMatrix());
  return maybe_mojo_from_native;
}

absl::optional<TransformationMatrix> XRSpace::NativeFromMojo() {
  absl::optional<TransformationMatrix> mojo_from_native = MojoFromNative();
  if (!mojo_from_native)
    return absl::nullopt;

  DCHECK(mojo_from_native->IsInvertible());
  return mojo_from_native->Inverse();
}

bool XRSpace::EmulatedPosition() const {
  return session()->EmulatedPosition();
}

XRPose* XRSpace::getPose(XRSpace* other_space) {
  DVLOG(2) << __func__ << ": ToString()=" << ToString()
           << ", other_space->ToString()=" << other_space->ToString();

  // Named mojo_from_offset because that is what we will leave it as, though it
  // starts mojo_from_native.
  absl::optional<TransformationMatrix> mojo_from_offset = MojoFromNative();
  if (!mojo_from_offset) {
    DVLOG(2) << __func__ << ": MojoFromNative() is not set";
    return nullptr;
  }

  // Add any origin offset now.
  mojo_from_offset->Multiply(NativeFromOffsetMatrix());

  absl::optional<TransformationMatrix> other_from_mojo =
      other_space->NativeFromMojo();
  if (!other_from_mojo) {
    DVLOG(2) << __func__ << ": other_space->NativeFromMojo() is not set";
    return nullptr;
  }

  // Add any origin offset from the other space now.
  TransformationMatrix other_offset_from_mojo =
      other_space->OffsetFromNativeMatrix().Multiply(*other_from_mojo);

  // TODO(crbug.com/969133): Update how EmulatedPosition is determined here once
  // spec issue https://github.com/immersive-web/webxr/issues/534 has been
  // resolved.
  TransformationMatrix other_offset_from_offset =
      other_offset_from_mojo.Multiply(*mojo_from_offset);
  return MakeGarbageCollected<XRPose>(
      other_offset_from_offset,
      EmulatedPosition() || other_space->EmulatedPosition());
}

absl::optional<TransformationMatrix> XRSpace::OffsetFromViewer() {
  absl::optional<TransformationMatrix> native_from_viewer =
      NativeFromViewer(session()->GetMojoFrom(
          device::mojom::blink::XRReferenceSpaceType::kViewer));

  if (!native_from_viewer) {
    return absl::nullopt;
  }

  return OffsetFromNativeMatrix().Multiply(*native_from_viewer);
}

ExecutionContext* XRSpace::GetExecutionContext() const {
  return session()->GetExecutionContext();
}

const AtomicString& XRSpace::InterfaceName() const {
  return event_target_names::kXRSpace;
}

void XRSpace::Trace(Visitor* visitor) const {
  visitor->Trace(session_);
  ScriptWrappable::Trace(visitor);
  EventTargetWithInlineData::Trace(visitor);
}

}  // namespace blink
