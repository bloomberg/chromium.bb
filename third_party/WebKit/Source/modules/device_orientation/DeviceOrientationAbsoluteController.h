// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DeviceOrientationAbsoluteController_h
#define DeviceOrientationAbsoluteController_h

#include "modules/ModulesExport.h"
#include "modules/device_orientation/DeviceOrientationController.h"

namespace blink {

class MODULES_EXPORT DeviceOrientationAbsoluteController final
    : public DeviceOrientationController {
 public:
  ~DeviceOrientationAbsoluteController() override;

  static const char* SupplementName();
  static DeviceOrientationAbsoluteController& From(Document&);

  // Inherited from DeviceSingleWindowEventController.
  void DidAddEventListener(LocalDOMWindow*,
                           const AtomicString& event_type) override;

  virtual void Trace(blink::Visitor*);

 private:
  explicit DeviceOrientationAbsoluteController(Document&);

  // Inherited from DeviceOrientationController.
  DeviceOrientationDispatcher& DispatcherInstance() const override;
  const AtomicString& EventTypeName() const override;
};

}  // namespace blink

#endif  // DeviceOrientationAbsoluteController_h
