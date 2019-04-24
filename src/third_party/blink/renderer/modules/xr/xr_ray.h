// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_RAY_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_RAY_H_

#include <memory>

#include "third_party/blink/renderer/core/geometry/dom_point_init.h"
#include "third_party/blink/renderer/core/geometry/dom_point_read_only.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/platform/geometry/float_point_3d.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class TransformationMatrix;
class XRRigidTransform;

class XRRay final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit XRRay(std::unique_ptr<TransformationMatrix> matrix);
  explicit XRRay(XRRigidTransform* transform);
  XRRay(DOMPointInit* origin, DOMPointInit* direction);
  ~XRRay() override;

  DOMPointReadOnly* origin() const { return origin_; }
  DOMPointReadOnly* direction() const { return direction_; }
  DOMFloat32Array* matrix();

  static XRRay* Create();
  static XRRay* Create(DOMPointInit* origin);
  static XRRay* Create(DOMPointInit* origin, DOMPointInit* direction);
  static XRRay* Create(XRRigidTransform* transform);

  void Trace(blink::Visitor*) override;

 private:
  void Set(std::unique_ptr<TransformationMatrix> matrix);
  void Set(FloatPoint3D origin, FloatPoint3D direction);

  Member<DOMPointReadOnly> origin_;
  Member<DOMPointReadOnly> direction_;
  std::unique_ptr<TransformationMatrix> matrix_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_RAY_H_
