// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_target_ray_space.h"

#include <utility>

#include "third_party/blink/renderer/modules/xr/xr_input_source.h"
#include "third_party/blink/renderer/modules/xr/xr_pose.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"

namespace blink {

XRTargetRaySpace::XRTargetRaySpace(XRSession* session, XRInputSource* source)
    : XRSpace(session), input_source_(source) {}

base::Optional<TransformationMatrix> XRTargetRaySpace::MojoFromNative() {
  auto mojo_from_viewer =
      session()->GetMojoFrom(XRReferenceSpace::Type::kTypeViewer);
  switch (input_source_->TargetRayMode()) {
    case device::mojom::XRTargetRayMode::TAPPING: {
      // If the pointer origin is the screen, we need mojo_from_viewer, as the
      // viewer space is the input space.
      // So our result will be mojo_from_viewer * viewer_from_pointer
      if (!(mojo_from_viewer && input_source_->InputFromPointer()))
        return base::nullopt;

      return *mojo_from_viewer * *(input_source_->InputFromPointer());
    }
    case device::mojom::XRTargetRayMode::GAZING: {
      // If the pointer origin is gaze, then the pointer offset is just
      // mojo_from_viewer.

      return mojo_from_viewer;
    }
    case device::mojom::XRTargetRayMode::POINTING: {
      // mojo_from_pointer is just: MojoFromInput*InputFromPointer;
      if (!(input_source_->MojoFromInput() &&
            input_source_->InputFromPointer()))
        return base::nullopt;

      return *(input_source_->MojoFromInput()) *
             *(input_source_->InputFromPointer());
    }
  }
}

base::Optional<TransformationMatrix> XRTargetRaySpace::NativeFromMojo() {
  return XRSpace::TryInvert(MojoFromNative());
}

bool XRTargetRaySpace::EmulatedPosition() const {
  return input_source_->emulatedPosition();
}

base::Optional<XRNativeOriginInformation> XRTargetRaySpace::NativeOrigin()
    const {
  return input_source_->nativeOrigin();
}

bool XRTargetRaySpace::IsStationary() const {
  // Target ray space is a space derived off of input source, so it is not
  // considered stationary.
  return false;
}

void XRTargetRaySpace::Trace(Visitor* visitor) {
  visitor->Trace(input_source_);
  XRSpace::Trace(visitor);
}

}  // namespace blink
