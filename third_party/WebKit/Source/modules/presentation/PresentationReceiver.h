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
#include "mojo/public/cpp/bindings/binding.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/heap/Heap.h"
#include "public/platform/modules/presentation/WebPresentationReceiver.h"
#include "public/platform/modules/presentation/presentation.mojom-blink.h"

namespace blink {

class Document;
class PresentationConnectionList;
class ReceiverPresentationConnection;
class WebPresentationClient;

// Implements the PresentationReceiver interface from the Presentation API from
// which websites can implement the receiving side of a presentation. This needs
// to be eagerly created in order to have the receiver associated with the
// client.
class MODULES_EXPORT PresentationReceiver final
    : public GarbageCollectedFinalized<PresentationReceiver>,
      public ScriptWrappable,
      public ContextLifecycleObserver,
      public WebPresentationReceiver,
      public mojom::blink::PresentationReceiver {
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

  // WebPresentationReceiver implementation.
  // Initializes the PresentationReceiver Mojo binding and registers itself as
  // a receiver with PresentationService. No-ops if already initialized.
  void Init() override;
  void OnReceiverTerminated() override;

  // mojom::blink::PresentationReceiver
  void OnReceiverConnectionAvailable(
      mojom::blink::PresentationInfoPtr,
      mojom::blink::PresentationConnectionPtr,
      mojom::blink::PresentationConnectionRequest) override;

  void RegisterConnection(ReceiverPresentationConnection*);
  void RemoveConnection(ReceiverPresentationConnection*);
  void Terminate();

  virtual void Trace(blink::Visitor*);

 private:
  friend class PresentationReceiverTest;

  static void RecordOriginTypeAccess(ExecutionContext&);

  // ContextLifecycleObserver implementation.
  void ContextDestroyed(ExecutionContext*) override;

  Member<ConnectionListProperty> connection_list_property_;
  Member<PresentationConnectionList> connection_list_;

  mojo::Binding<mojom::blink::PresentationReceiver> receiver_binding_;
  WebPresentationClient* client_;
};

}  // namespace blink

#endif  // PresentationReceiver_h
