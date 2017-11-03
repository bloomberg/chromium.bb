// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VRLayer_h
#define VRLayer_h

#include "modules/vr/latest/VRView.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Forward.h"

namespace blink {

class VRSession;
class VRViewport;

enum VRLayerType { kVRWebGLLayerType };

class VRLayer : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  VRLayer(VRSession*, VRLayerType);

  VRSession* session() const { return session_; }
  VRLayerType layerType() const { return layer_type_; }
  virtual VRViewport* GetViewport(VRView::Eye);

  virtual void OnFrameStart();
  virtual void OnFrameEnd();
  virtual void OnResize();

  virtual void Trace(blink::Visitor*);

 private:
  const Member<VRSession> session_;
  VRLayerType layer_type_;
};

}  // namespace blink

#endif  // VRLayer_h
