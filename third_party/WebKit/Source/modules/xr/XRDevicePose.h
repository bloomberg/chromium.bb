// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef XRDevicePose_h
#define XRDevicePose_h

#include "core/typed_arrays/DOMTypedArray.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/transforms/TransformationMatrix.h"

namespace blink {

class XRSession;
class XRView;

class XRDevicePose final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  XRDevicePose(XRSession*, std::unique_ptr<TransformationMatrix>);

  DOMFloat32Array* poseModelMatrix() const;
  DOMFloat32Array* getViewMatrix(XRView*);

  virtual void Trace(blink::Visitor*);

 private:
  const Member<XRSession> session_;
  std::unique_ptr<TransformationMatrix> pose_model_matrix_;
};

}  // namespace blink

#endif  // XRWebGLLayer_h
