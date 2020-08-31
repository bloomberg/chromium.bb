// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/overlay_processor_stub.h"

namespace viz {
bool OverlayProcessorStub::IsOverlaySupported() const {
  return false;
}
gfx::Rect OverlayProcessorStub::GetAndResetOverlayDamage() {
  return gfx::Rect();
}
bool OverlayProcessorStub::NeedsSurfaceOccludingDamageRect() const {
  return false;
}

}  // namespace viz
