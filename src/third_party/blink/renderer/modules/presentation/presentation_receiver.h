// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PRESENTATION_PRESENTATION_RECEIVER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PRESENTATION_PRESENTATION_RECEIVER_H_

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/presentation/presentation.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_property.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"

namespace blink {

class Document;
class PresentationConnectionList;
class ReceiverPresentationConnection;

// Implements the PresentationReceiver interface from the Presentation API from
// which websites can implement the receiving side of a presentation. This needs
// to be eagerly created in order to have the receiver associated with the
// client.
class MODULES_EXPORT PresentationReceiver final
    : public ScriptWrappable,
      public mojom::blink::PresentationReceiver {
  DEFINE_WRAPPERTYPEINFO();
  using ConnectionListProperty =
      ScriptPromiseProperty<Member<PresentationConnectionList>,
                            Member<DOMException>>;

 public:
  explicit PresentationReceiver(LocalDOMWindow*);
  ~PresentationReceiver() override = default;

  static PresentationReceiver* From(Document&);

  // PresentationReceiver.idl implementation
  ScriptPromise connectionList(ScriptState*);

  // mojom::blink::PresentationReceiver
  void OnReceiverConnectionAvailable(
      mojom::blink::PresentationInfoPtr,
      mojo::PendingRemote<mojom::blink::PresentationConnection>,
      mojo::PendingReceiver<mojom::blink::PresentationConnection>) override;

  void RegisterConnection(ReceiverPresentationConnection*);
  void RemoveConnection(ReceiverPresentationConnection*);
  void Terminate();

  LocalDOMWindow* GetWindow() const { return window_; }

  void Trace(Visitor*) override;

 private:
  friend class PresentationReceiverTest;

  static void RecordOriginTypeAccess(ExecutionContext&);

  Member<ConnectionListProperty> connection_list_property_;
  Member<PresentationConnectionList> connection_list_;

  HeapMojoReceiver<mojom::blink::PresentationReceiver, PresentationReceiver>
      presentation_receiver_receiver_;
  HeapMojoRemote<mojom::blink::PresentationService>
      presentation_service_remote_;
  Member<LocalDOMWindow> window_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PRESENTATION_PRESENTATION_RECEIVER_H_
