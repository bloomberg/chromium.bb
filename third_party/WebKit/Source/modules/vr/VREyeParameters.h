// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VREyeParameters_h
#define VREyeParameters_h

#include "core/typed_arrays/DOMTypedArray.h"
#include "device/vr/vr_service.mojom-blink.h"
#include "modules/vr/VRFieldOfView.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"

#include "platform/wtf/Forward.h"

namespace blink {

class VREyeParameters final : public GarbageCollected<VREyeParameters>,
                              public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  VREyeParameters();

  DOMFloat32Array* offset() const { return offset_; }
  VRFieldOfView* FieldOfView() const { return field_of_view_; }
  unsigned long renderWidth() const { return render_width_; }
  unsigned long renderHeight() const { return render_height_; }

  void Update(const device::mojom::blink::VREyeParametersPtr&);

  DECLARE_VIRTUAL_TRACE()

 private:
  Member<DOMFloat32Array> offset_;
  Member<VRFieldOfView> field_of_view_;
  unsigned long render_width_;
  unsigned long render_height_;
};

}  // namespace blink

#endif  // VREyeParameters_h
