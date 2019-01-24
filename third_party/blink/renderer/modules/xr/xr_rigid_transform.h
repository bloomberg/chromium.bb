// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_RIGID_TRANSFORM_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_RIGID_TRANSFORM_H_

#include <memory>

#include "third_party/blink/renderer/core/geometry/dom_point_init.h"
#include "third_party/blink/renderer/core/geometry/dom_point_read_only.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/transforms/transformation_matrix.h"

namespace blink {

class XRRigidTransform : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  XRRigidTransform(DOMPointInit*, DOMPointInit*);
  static XRRigidTransform* Create(DOMPointInit*, DOMPointInit*);

  DOMPointReadOnly* position() const { return position_; }
  DOMPointReadOnly* orientation() const { return orientation_; }
  DOMFloat32Array* matrix();

  void Trace(blink::Visitor*) override;

 private:
  Member<DOMPointReadOnly> position_;
  Member<DOMPointReadOnly> orientation_;
  std::unique_ptr<TransformationMatrix> matrix_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_RIGID_TRANSFORM_H_
