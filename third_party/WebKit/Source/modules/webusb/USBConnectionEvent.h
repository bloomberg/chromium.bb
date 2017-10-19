// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef USBConnectionEvent_h
#define USBConnectionEvent_h

#include "modules/EventModules.h"
#include "platform/heap/Handle.h"

namespace blink {

class USBConnectionEventInit;
class USBDevice;

class USBConnectionEvent final : public Event {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static USBConnectionEvent* Create(const AtomicString& type,
                                    const USBConnectionEventInit&);
  static USBConnectionEvent* Create(const AtomicString& type, USBDevice*);

  USBConnectionEvent(const AtomicString& type, const USBConnectionEventInit&);
  USBConnectionEvent(const AtomicString& type, USBDevice*);

  USBDevice* device() const { return device_; }

  virtual void Trace(blink::Visitor*);

 private:
  Member<USBDevice> device_;
};

}  // namespace blink

#endif  // USBConnectionEvent_h
