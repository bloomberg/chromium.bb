// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PresentationConnectionList_h
#define PresentationConnectionList_h

#include "core/dom/ExecutionContext.h"
#include "core/events/EventTarget.h"
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
  const AtomicString& interfaceName() const override;
  ExecutionContext* getExecutionContext() const override {
    return ContextClient::getExecutionContext();
  }

  // PresentationConnectionList.idl implementation.
  const HeapVector<Member<PresentationConnection>>& connections() const;
  DEFINE_ATTRIBUTE_EVENT_LISTENER(connectionavailable);

  void addConnection(PresentationConnection*);
  void dispatchConnectionAvailableEvent(PresentationConnection*);
  bool isEmpty();

  DECLARE_VIRTUAL_TRACE();

 protected:
  // EventTarget implementation.
  void addedEventListener(const AtomicString& eventType,
                          RegisteredEventListener&) override;

 private:
  friend class PresentationReceiverTest;

  HeapVector<Member<PresentationConnection>> m_connections;
};

}  // namespace blink

#endif  // PresentationConnectionList_h
