// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VRCoordinateSystem_h
#define VRCoordinateSystem_h

#include "core/typed_arrays/DOMTypedArray.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Forward.h"

namespace blink {

class TransformationMatrix;
class VRSession;

class VRCoordinateSystem : public GarbageCollectedFinalized<VRCoordinateSystem>,
                           public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit VRCoordinateSystem(VRSession*);
  virtual ~VRCoordinateSystem();

  DOMFloat32Array* getTransformTo(VRCoordinateSystem*) const;

  VRSession* session() { return session_; }

  virtual std::unique_ptr<TransformationMatrix> TransformBasePose(
      const TransformationMatrix& base_pose) = 0;

  DECLARE_VIRTUAL_TRACE();

 private:
  const Member<VRSession> session_;
};

}  // namespace blink

#endif  // VRWebGLLayer_h
