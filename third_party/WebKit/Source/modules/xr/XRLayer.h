// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef XRLayer_h
#define XRLayer_h

#include "modules/xr/XRView.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Forward.h"

namespace blink {

class XRSession;
class XRViewport;

enum XRLayerType { kXRWebGLLayerType };

class XRLayer : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  XRLayer(XRSession*, XRLayerType);

  XRSession* session() const { return session_; }
  XRLayerType layerType() const { return layer_type_; }
  virtual XRViewport* GetViewport(XRView::Eye);

  virtual void OnFrameStart();
  virtual void OnFrameEnd();
  virtual void OnResize();

  virtual void Trace(blink::Visitor*);

 private:
  const Member<XRSession> session_;
  XRLayerType layer_type_;
};

}  // namespace blink

#endif  // XRLayer_h
