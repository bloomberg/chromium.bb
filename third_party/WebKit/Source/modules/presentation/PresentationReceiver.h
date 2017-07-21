// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PresentationReceiver_h
#define PresentationReceiver_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseProperty.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "core/dom/DOMException.h"
#include "modules/ModulesExport.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/heap/Heap.h"
#include "public/platform/modules/presentation/WebPresentationReceiver.h"

namespace blink {

class Document;
class PresentationConnection;
class PresentationConnectionList;
class WebPresentationClient;

// Implements the PresentationReceiver interface from the Presentation API from
// which websites can implement the receiving side of a presentation. This needs
// to be eagerly created in order to have the receiver associated with the
// client.
class MODULES_EXPORT PresentationReceiver final
    : public GarbageCollectedFinalized<PresentationReceiver>,
      public ScriptWrappable,
      public ContextClient,
      public WebPresentationReceiver {
  USING_GARBAGE_COLLECTED_MIXIN(PresentationReceiver);
  DEFINE_WRAPPERTYPEINFO();
  using ConnectionListProperty =
      ScriptPromiseProperty<Member<PresentationReceiver>,
                            Member<PresentationConnectionList>,
                            Member<DOMException>>;

 public:
  PresentationReceiver(LocalFrame*, WebPresentationClient*);
  ~PresentationReceiver() = default;

  static PresentationReceiver* From(Document&);

  // PresentationReceiver.idl implementation
  ScriptPromise connectionList(ScriptState*);

  // Implementation of WebPresentationController.
  WebPresentationConnection* OnReceiverConnectionAvailable(
      const WebPresentationInfo&) override;
  void DidChangeConnectionState(WebPresentationConnectionState) override;
  void TerminateConnection() override;
  void RemoveConnection(WebPresentationConnection*) override;

  void RegisterConnection(PresentationConnection*);

  DECLARE_VIRTUAL_TRACE();

 private:
  friend class PresentationReceiverTest;

  static void RecordOriginTypeAccess(ExecutionContext&);

  Member<ConnectionListProperty> connection_list_property_;
  Member<PresentationConnectionList> connection_list_;
};

}  // namespace blink

#endif  // PresentationReceiver_h
