// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DeviceOrientationInspectorAgent_h
#define DeviceOrientationInspectorAgent_h

#include "core/inspector/InspectorBaseAgent.h"
#include "core/inspector/protocol/DeviceOrientation.h"
#include "modules/ModulesExport.h"

namespace blink {

class DeviceOrientationController;
class InspectedFrames;

class MODULES_EXPORT DeviceOrientationInspectorAgent final
    : public InspectorBaseAgent<protocol::DeviceOrientation::Metainfo> {
  WTF_MAKE_NONCOPYABLE(DeviceOrientationInspectorAgent);

 public:
  explicit DeviceOrientationInspectorAgent(InspectedFrames*);
  ~DeviceOrientationInspectorAgent() override;
  virtual void Trace(blink::Visitor*);

  // Protocol methods.
  protocol::Response setDeviceOrientationOverride(double,
                                                  double,
                                                  double) override;
  protocol::Response clearDeviceOrientationOverride() override;

  protocol::Response disable() override;
  void Restore() override;
  void DidCommitLoadForLocalFrame(LocalFrame*) override;

 private:
  DeviceOrientationController* Controller();

  Member<InspectedFrames> inspected_frames_;
};

}  // namespace blink

#endif  // !defined(DeviceOrientationInspectorAgent_h)
