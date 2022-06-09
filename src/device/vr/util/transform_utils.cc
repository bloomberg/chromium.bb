// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/util/transform_utils.h"

#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/transform_util.h"

namespace device {
namespace vr_utils {

gfx::Transform VrPoseToTransform(const device::mojom::VRPose* pose) {
  gfx::DecomposedTransform decomp;
  if (pose->orientation) {
    decomp.quaternion =
        gfx::Quaternion(pose->orientation->x(), pose->orientation->y(),
                        pose->orientation->z(), pose->orientation->w());
  }
  if (pose->position) {
    decomp.translate[0] = pose->position->x();
    decomp.translate[1] = pose->position->y();
    decomp.translate[2] = pose->position->z();
  }

  return gfx::ComposeTransform(decomp);
}

}  // namespace vr_utils
}  // namespace device
