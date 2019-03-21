// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_RIGID_TRANSFORM_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_RIGID_TRANSFORM_H_

#include <memory>
#include <utility>

#include "third_party/blink/renderer/core/geometry/dom_point_init.h"
#include "third_party/blink/renderer/core/geometry/dom_point_read_only.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/transforms/transformation_matrix.h"

namespace blink {

// MODULES_EXPORT is required for unit tests using XRRigidTransform (currently
// just xr_rigid_transform_test.cc) to build without linker errors.
class MODULES_EXPORT XRRigidTransform : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // deep copies
  XRRigidTransform(const XRRigidTransform& other);
  XRRigidTransform& operator=(const XRRigidTransform& other);

  explicit XRRigidTransform(const TransformationMatrix&);
  explicit XRRigidTransform(std::unique_ptr<TransformationMatrix>);
  XRRigidTransform(DOMPointInit*, DOMPointInit*);
  static XRRigidTransform* Create(DOMPointInit*, DOMPointInit*);

  ~XRRigidTransform() override = default;

  DOMPointReadOnly* position() const { return position_; }
  DOMPointReadOnly* orientation() const { return orientation_; }
  DOMFloat32Array* matrix();
  XRRigidTransform* inverse();

  TransformationMatrix InverseTransformMatrix();
  TransformationMatrix TransformMatrix();  // copies matrix_

  void Trace(blink::Visitor*) override;

 private:
  void DecomposeMatrix();
  void EnsureMatrix();

  Member<DOMPointReadOnly> position_;
  Member<DOMPointReadOnly> orientation_;
  std::unique_ptr<TransformationMatrix> matrix_;
  std::unique_ptr<TransformationMatrix> inv_matrix_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_RIGID_TRANSFORM_H_
