// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef XRCoordinateSystem_h
#define XRCoordinateSystem_h

#include "core/typed_arrays/DOMTypedArray.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Forward.h"

namespace blink {

class TransformationMatrix;
class XRSession;

class XRCoordinateSystem : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit XRCoordinateSystem(XRSession*);
  virtual ~XRCoordinateSystem();

  DOMFloat32Array* getTransformTo(XRCoordinateSystem*) const;

  XRSession* session() { return session_; }

  virtual std::unique_ptr<TransformationMatrix> TransformBasePose(
      const TransformationMatrix& base_pose) = 0;

  virtual void Trace(blink::Visitor*);

 private:
  const Member<XRSession> session_;
};

}  // namespace blink

#endif  // XRWebGLLayer_h
