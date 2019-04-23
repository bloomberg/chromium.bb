// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_WORLD_INFORMATION_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_WORLD_INFORMATION_H_

#include "third_party/blink/renderer/modules/xr/xr_plane.h"

namespace blink {

class XRWorldInformation : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  HeapVector<Member<XRPlane>> detectedPlanes(bool& is_null) const {
    is_null = true;
    return {};
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_WORLD_INFORMATION_H_
