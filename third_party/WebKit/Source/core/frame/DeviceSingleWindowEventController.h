// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DeviceSingleWindowEventController_h
#define DeviceSingleWindowEventController_h

#include "core/CoreExport.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/PlatformEventController.h"
#include "platform/heap/Handle.h"

namespace blink {

class Document;
class Event;

class CORE_EXPORT DeviceSingleWindowEventController
    : public GarbageCollectedFinalized<DeviceSingleWindowEventController>,
      public PlatformEventController,
      public LocalDOMWindow::EventListenerObserver {
 public:
  virtual ~DeviceSingleWindowEventController();

  // Inherited from DeviceEventControllerBase.
  void DidUpdateData() override;
  virtual void Trace(blink::Visitor*);

  // Inherited from LocalDOMWindow::EventListenerObserver.
  void DidAddEventListener(LocalDOMWindow*, const AtomicString&) override;
  void DidRemoveEventListener(LocalDOMWindow*, const AtomicString&) override;
  void DidRemoveAllEventListeners(LocalDOMWindow*) override;

 protected:
  explicit DeviceSingleWindowEventController(Document&);

  Document& GetDocument() const { return *document_; }
  bool IsSameSecurityOriginAsMainFrame() const;

  void DispatchDeviceEvent(Event*);

  virtual Event* LastEvent() const = 0;
  virtual const AtomicString& EventTypeName() const = 0;
  virtual bool IsNullEvent(Event*) const = 0;

 private:
  bool needs_checking_null_events_;
  Member<Document> document_;
};

}  // namespace blink

#endif  // DeviceSingleWindowEventController_h
