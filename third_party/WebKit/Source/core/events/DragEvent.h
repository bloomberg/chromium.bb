// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DragEvent_h
#define DragEvent_h

#include "core/CoreExport.h"
#include "core/events/DragEventInit.h"
#include "core/events/MouseEvent.h"

namespace blink {

class DataTransfer;

class CORE_EXPORT DragEvent final : public MouseEvent {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static DragEvent* Create() { return new DragEvent; }

  static DragEvent* Create(const AtomicString& type,
                           const DragEventInit& initializer,
                           TimeTicks platform_time_stamp,
                           SyntheticEventType synthetic_event_type) {
    return new DragEvent(type, initializer, platform_time_stamp,
                         synthetic_event_type);
  }

  static DragEvent* Create(const AtomicString& type,
                           const DragEventInit& initializer) {
    return new DragEvent(type, initializer, CurrentTimeTicks(),
                         kRealOrIndistinguishable);
  }

  DataTransfer* getDataTransfer() const override {
    return IsDragEvent() ? data_transfer_.Get() : nullptr;
  }

  bool IsDragEvent() const override;
  bool IsMouseEvent() const override;

  DispatchEventResult DispatchEvent(EventDispatcher&) override;

  virtual void Trace(blink::Visitor*);

 private:
  DragEvent();
  DragEvent(const AtomicString& type,
            const DragEventInit&,
            TimeTicks platform_time_stamp,
            SyntheticEventType);

  Member<DataTransfer> data_transfer_;
};

DEFINE_EVENT_TYPE_CASTS(DragEvent);

}  // namespace blink

#endif  // DragEvent_h
