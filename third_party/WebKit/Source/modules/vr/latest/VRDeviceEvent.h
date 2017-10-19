// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VRDeviceEvent_h
#define VRDeviceEvent_h

#include "modules/EventModules.h"
#include "modules/vr/latest/VRDevice.h"
#include "modules/vr/latest/VRDeviceEventInit.h"

namespace blink {

class VRDeviceEvent final : public Event {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static VRDeviceEvent* Create() { return new VRDeviceEvent; }
  static VRDeviceEvent* Create(const AtomicString& type, VRDevice* device) {
    return new VRDeviceEvent(type, device);
  }

  static VRDeviceEvent* Create(const AtomicString& type,
                               const VRDeviceEventInit& initializer) {
    return new VRDeviceEvent(type, initializer);
  }

  ~VRDeviceEvent() override;

  VRDevice* device() const { return device_.Get(); }

  const AtomicString& InterfaceName() const override;

  virtual void Trace(blink::Visitor*);

 private:
  VRDeviceEvent();
  VRDeviceEvent(const AtomicString& type, VRDevice*);
  VRDeviceEvent(const AtomicString&, const VRDeviceEventInit&);

  Member<VRDevice> device_;
};

}  // namespace blink

#endif  // VRDisplayEvent_h
