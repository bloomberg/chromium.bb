// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef XRInputPose_h
#define XRInputPose_h

#include "core/typed_arrays/DOMTypedArray.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/transforms/TransformationMatrix.h"
#include "platform/wtf/Forward.h"

namespace blink {

class XRInputPose final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  XRInputPose(std::unique_ptr<TransformationMatrix> pointer_matrix,
              std::unique_ptr<TransformationMatrix> grip_matrix,
              bool emulated_position = false);
  ~XRInputPose();

  DOMFloat32Array* pointerMatrix() const;
  DOMFloat32Array* gripMatrix() const;
  bool emulatedPosition() const { return emulated_position_; }

  virtual void Trace(blink::Visitor*);

 private:
  const std::unique_ptr<TransformationMatrix> pointer_matrix_;
  const std::unique_ptr<TransformationMatrix> grip_matrix_;
  const bool emulated_position_;
};

}  // namespace blink

#endif  // XRInputPose_h
