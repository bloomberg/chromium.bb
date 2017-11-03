// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VRView_h
#define VRView_h

#include "core/typed_arrays/DOMTypedArray.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/geometry/FloatPoint3D.h"
#include "platform/heap/Handle.h"
#include "platform/transforms/TransformationMatrix.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class VRLayer;
class VRSession;
class VRViewport;

class VRView final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  enum Eye { kEyeLeft = 0, kEyeRight = 1 };

  VRView(VRSession*, Eye);

  const String& eye() const { return eye_string_; }
  VRSession* session() const;
  DOMFloat32Array* projectionMatrix() const { return projection_matrix_; }
  VRViewport* getViewport(VRLayer*) const;

  void UpdateProjectionMatrixFromFoV(float up_rad,
                                     float down_rad,
                                     float left_rad,
                                     float right_rad,
                                     float near_depth,
                                     float far_depth);

  // TODO(bajones): Should eventually represent this as a full transform.
  const FloatPoint3D& offset() const { return offset_; }
  void UpdateOffset(float x, float y, float z);

  virtual void Trace(blink::Visitor*);

 private:
  const Eye eye_;
  String eye_string_;
  Member<VRSession> session_;
  Member<DOMFloat32Array> projection_matrix_;
  FloatPoint3D offset_;
};

}  // namespace blink

#endif  // VRView_h
