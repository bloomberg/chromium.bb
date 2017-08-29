// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PresentationConnectionList_h
#define PresentationConnectionList_h

#include "core/dom/ExecutionContext.h"
#include "core/dom/events/EventTarget.h"
#include "modules/ModulesExport.h"
#include "modules/presentation/PresentationConnection.h"
#include "platform/heap/Handle.h"

namespace blink {

// Implements the PresentationConnectionList interface from the Presentation API
// from which represents set of presentation connections in the set of
// presentation controllers.
class MODULES_EXPORT PresentationConnectionList final
    : public EventTargetWithInlineData,
      public ContextClient {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(PresentationConnectionList);

 public:
  explicit PresentationConnectionList(ExecutionContext*);
  ~PresentationConnectionList() = default;

  // EventTarget implementation.
  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override {
    return ContextClient::GetExecutionContext();
  }

  // PresentationConnectionList.idl implementation.
  const HeapVector<Member<PresentationConnection>>& connections() const;
  DEFINE_ATTRIBUTE_EVENT_LISTENER(connectionavailable);

  void AddConnection(PresentationConnection*);
  // Remove connection from connection list. Returns true if connection is
  // successfully removed; Returns false if connection does not exist.
  bool RemoveConnection(WebPresentationConnection*);
  void DispatchConnectionAvailableEvent(PresentationConnection*);
  bool IsEmpty();

  DECLARE_VIRTUAL_TRACE();

 protected:
  // EventTarget implementation.
  void AddedEventListener(const AtomicString& event_type,
                          RegisteredEventListener&) override;

 private:
  friend class PresentationReceiverTest;

  HeapVector<Member<PresentationConnection>> connections_;
};

}  // namespace blink

#endif  // PresentationConnectionList_h
