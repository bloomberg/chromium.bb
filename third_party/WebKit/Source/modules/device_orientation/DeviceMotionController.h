// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DeviceMotionController_h
#define DeviceMotionController_h

#include "core/frame/DeviceSingleWindowEventController.h"
#include "modules/ModulesExport.h"

namespace blink {

class Event;

class MODULES_EXPORT DeviceMotionController final
    : public DeviceSingleWindowEventController,
      public Supplement<Document> {
  USING_GARBAGE_COLLECTED_MIXIN(DeviceMotionController);

 public:
  ~DeviceMotionController() override;

  static const char* SupplementName();
  static DeviceMotionController& From(Document&);

  // DeviceSingleWindowEventController
  void DidAddEventListener(LocalDOMWindow*,
                           const AtomicString& event_type) override;

  virtual void Trace(blink::Visitor*);

 private:
  explicit DeviceMotionController(Document&);

  // Inherited from DeviceEventControllerBase.
  void RegisterWithDispatcher() override;
  void UnregisterWithDispatcher() override;
  bool HasLastData() override;

  // Inherited from DeviceSingleWindowEventController.
  Event* LastEvent() const override;
  const AtomicString& EventTypeName() const override;
  bool IsNullEvent(Event*) const override;
};

}  // namespace blink

#endif  // DeviceMotionController_h
