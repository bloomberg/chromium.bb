// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef XRDeviceEvent_h
#define XRDeviceEvent_h

#include "modules/EventModules.h"
#include "modules/xr/XRDevice.h"
#include "modules/xr/XRDeviceEventInit.h"

namespace blink {

class XRDeviceEvent final : public Event {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static XRDeviceEvent* Create() { return new XRDeviceEvent; }
  static XRDeviceEvent* Create(const AtomicString& type, XRDevice* device) {
    return new XRDeviceEvent(type, device);
  }

  static XRDeviceEvent* Create(const AtomicString& type,
                               const XRDeviceEventInit& initializer) {
    return new XRDeviceEvent(type, initializer);
  }

  ~XRDeviceEvent() override;

  XRDevice* device() const { return device_.Get(); }

  const AtomicString& InterfaceName() const override;

  void Trace(blink::Visitor*) override;

 private:
  XRDeviceEvent();
  XRDeviceEvent(const AtomicString& type, XRDevice*);
  XRDeviceEvent(const AtomicString&, const XRDeviceEventInit&);

  Member<XRDevice> device_;
};

}  // namespace blink

#endif  // XRDisplayEvent_h
