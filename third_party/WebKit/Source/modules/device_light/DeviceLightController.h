// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DeviceLightController_h
#define DeviceLightController_h

#include "core/dom/Document.h"
#include "core/frame/DeviceSingleWindowEventController.h"
#include "modules/ModulesExport.h"

namespace blink {

class Event;

class MODULES_EXPORT DeviceLightController final
    : public DeviceSingleWindowEventController,
      public Supplement<Document> {
  USING_GARBAGE_COLLECTED_MIXIN(DeviceLightController);

 public:
  ~DeviceLightController() override;

  static const char* SupplementName();
  static DeviceLightController& From(Document&);

  DECLARE_VIRTUAL_TRACE();

 private:
  explicit DeviceLightController(Document&);

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

#endif  // DeviceLightController_h
