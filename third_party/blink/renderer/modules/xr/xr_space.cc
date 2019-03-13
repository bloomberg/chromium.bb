// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_space.h"

#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/xr/xr_input_source.h"
#include "third_party/blink/renderer/modules/xr/xr_rigid_transform.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"

namespace blink {

XRSpace::XRSpace(XRSession* session)
    : session_(session), input_source_(nullptr), return_target_ray_(false) {}

XRSpace::~XRSpace() = default;

void XRSpace::SetInputSource(XRInputSource* source, bool return_target_ray) {
  input_source_ = source;
  return_target_ray_ = return_target_ray;
}

std::unique_ptr<TransformationMatrix> XRSpace::GetTransformToMojoSpace() {
  // The base XRSpace does not have any relevant information, so can't determine
  // a transform here.
  return nullptr;
}

std::unique_ptr<TransformationMatrix> XRSpace::DefaultPose() {
  return nullptr;
}

std::unique_ptr<TransformationMatrix> XRSpace::TransformBasePose(
    const TransformationMatrix& base_pose) {
  return nullptr;
}

std::unique_ptr<TransformationMatrix> XRSpace::TransformBaseInputPose(
    const TransformationMatrix& base_input_pose,
    const TransformationMatrix& base_pose) {
  return nullptr;
}

TransformationMatrix XRSpace::OriginOffsetMatrix() {
  TransformationMatrix identity;
  return identity;
}

ExecutionContext* XRSpace::GetExecutionContext() const {
  return session()->GetExecutionContext();
}

const AtomicString& XRSpace::InterfaceName() const {
  return event_target_names::kXRSpace;
}

void XRSpace::Trace(blink::Visitor* visitor) {
  visitor->Trace(input_source_);
  visitor->Trace(session_);
  ScriptWrappable::Trace(visitor);
  EventTargetWithInlineData::Trace(visitor);
}

}  // namespace blink
