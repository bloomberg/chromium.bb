// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef XRPresentationFrame_h
#define XRPresentationFrame_h

#include "device/vr/vr_service.mojom-blink.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/bindings/TraceWrapperMember.h"
#include "platform/heap/Handle.h"
#include "platform/transforms/TransformationMatrix.h"
#include "platform/wtf/Forward.h"

namespace blink {

class XRCoordinateSystem;
class XRDevicePose;
class XRSession;
class XRView;

class XRPresentationFrame final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit XRPresentationFrame(XRSession*);

  XRSession* session() const { return session_; }

  const HeapVector<Member<XRView>>& views() const;
  XRDevicePose* getDevicePose(XRCoordinateSystem*) const;

  void UpdateBasePose(std::unique_ptr<TransformationMatrix>);

  virtual void Trace(blink::Visitor*);

 private:
  const Member<XRSession> session_;
  std::unique_ptr<TransformationMatrix> base_pose_matrix_;
};

}  // namespace blink

#endif  // XRWebGLLayer_h
