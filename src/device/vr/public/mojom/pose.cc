// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/public/mojom/pose.h"

#include "ui/gfx/transform_util.h"

namespace device {

Pose::Pose() = default;

Pose::Pose(const gfx::Point3F& position, const gfx::Quaternion& orientation)
    : position_(position), orientation_(orientation) {
  gfx::DecomposedTransform decomposed_pose;
  decomposed_pose.translate[0] = position.x();
  decomposed_pose.translate[1] = position.y();
  decomposed_pose.translate[2] = position.z();
  decomposed_pose.quaternion = orientation;

  other_from_this_ = gfx::ComposeTransform(decomposed_pose);
}

base::Optional<Pose> Pose::Create(const gfx::Transform& other_from_this) {
  gfx::DecomposedTransform decomposed_other_from_this;
  if (!gfx::DecomposeTransform(&decomposed_other_from_this, other_from_this)) {
    return base::nullopt;
  }

  return Pose(gfx::Point3F(decomposed_other_from_this.translate[0],
                           decomposed_other_from_this.translate[1],
                           decomposed_other_from_this.translate[2]),
              decomposed_other_from_this.quaternion);
}

const gfx::Transform& Pose::ToTransform() const {
  return other_from_this_;
}

}  // namespace device
