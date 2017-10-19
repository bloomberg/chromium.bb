// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PresentationConnection_h
#define PresentationConnection_h

#include <memory>
#include "core/dom/ContextLifecycleObserver.h"
#include "core/dom/events/EventTarget.h"
#include "core/fileapi/Blob.h"
#include "core/fileapi/FileError.h"
#include "core/typed_arrays/ArrayBufferViewHelpers.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "platform/heap/Handle.h"
#include "platform/weborigin/KURL.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/modules/presentation/WebPresentationConnection.h"
#include "public/platform/modules/presentation/WebPresentationController.h"
#include "public/platform/modules/presentation/WebPresentationInfo.h"
#include "public/platform/modules/presentation/presentation.mojom-blink.h"

namespace WTF {
class AtomicString;
}  // namespace WTF

namespace blink {

class DOMArrayBuffer;
class DOMArrayBufferView;
class PresentationController;
class PresentationReceiver;
class PresentationRequest;

class PresentationConnection : public EventTargetWithInlineData,
                               public ContextLifecycleObserver,
                               public mojom::blink::PresentationConnection {
  USING_GARBAGE_COLLECTED_MIXIN(PresentationConnection);
  DEFINE_WRAPPERTYPEINFO();

 public:
  ~PresentationConnection() override;

  // EventTarget implementation.
  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;

  virtual void Trace(blink::Visitor*);

  const String& id() const { return id_; }
  const String& url() const { return url_; }
  const WTF::AtomicString& state() const;

  void send(const String& message, ExceptionState&);
  void send(DOMArrayBuffer*, ExceptionState&);
  void send(NotShared<DOMArrayBufferView>, ExceptionState&);
  void send(Blob*, ExceptionState&);
  void close();
  void terminate();

  String binaryType() const;
  void setBinaryType(const String&);

  DEFINE_ATTRIBUTE_EVENT_LISTENER(message);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(connect);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(close);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(terminate);

  // Returns true if and only if the the presentation info matches this
  // connection.
  bool Matches(const WebPresentationInfo&) const;

  // Returns true if this connection's id equals to |id| and its url equals to
  // |url|.
  bool Matches(const String& id, const KURL&) const;

  // Notifies the connection about its state change to 'closed'.
  void DidClose(WebPresentationConnectionCloseReason, const String& message);

  // mojom::blink::PresentationConnection implementation.
  void OnMessage(mojom::blink::PresentationConnectionMessagePtr,
                 OnMessageCallback) override;
  void DidChangeState(mojom::blink::PresentationConnectionState) override;
  void RequestClose() override;

  mojom::blink::PresentationConnectionState GetState() const;

 protected:
  static void DispatchEventAsync(EventTarget*, Event*);

  PresentationConnection(LocalFrame&, const String& id, const KURL&);

  // EventTarget implementation.
  void AddedEventListener(const AtomicString& event_type,
                          RegisteredEventListener&) override;

  // ContextLifecycleObserver implementation.
  void ContextDestroyed(ExecutionContext*) override;

  String id_;
  KURL url_;
  mojom::blink::PresentationConnectionState state_;

  mojo::Binding<mojom::blink::PresentationConnection> connection_binding_;

  // The other end of a PresentationConnection. For controller connections, this
  // can point to the browser (2-UA) or another renderer (1-UA). For receiver
  // connections, this currently only points to another renderer. This ptr can
  // be used to send messages directly to the other end.
  mojom::blink::PresentationConnectionPtr target_connection_;

 private:
  class BlobLoader;

  enum MessageType {
    kMessageTypeText,
    kMessageTypeArrayBuffer,
    kMessageTypeBlob,
  };

  enum BinaryType { kBinaryTypeBlob, kBinaryTypeArrayBuffer };

  class Message;

  // Implemented by controller/receiver subclasses to perform additional
  // operations.
  virtual void DoClose() = 0;
  virtual void DoTerminate() = 0;

  bool CanSendMessage(ExceptionState&);
  void HandleMessageQueue();

  // Callbacks invoked from BlobLoader.
  void DidFinishLoadingBlob(DOMArrayBuffer*);
  void DidFailLoadingBlob(FileError::ErrorCode);

  void SendMessageToTargetConnection(
      mojom::blink::PresentationConnectionMessagePtr);
  void DidReceiveTextMessage(const WebString&);
  void DidReceiveBinaryMessage(const uint8_t*, size_t length);

  // Notifies the presentation about its state change to 'closed', with
  // "closed" being the reason and empty string as the message.
  void DidClose();

  // Internal helper function to dispatch state change events asynchronously.
  void DispatchStateChangeEvent(Event*);

  // Cancel loads and pending messages when the connection is closed.
  void TearDown();

  // For Blob data handling.
  Member<BlobLoader> blob_loader_;
  HeapDeque<Member<Message>> messages_;

  BinaryType binary_type_;
};

// Represents the controller side of a connection of either a 1-UA or 2-UA
// presentation.
class ControllerPresentationConnection final
    : public PresentationConnection,
      public WebPresentationConnection {
 public:
  // For CallbackPromiseAdapter.
  static ControllerPresentationConnection* Take(ScriptPromiseResolver*,
                                                const WebPresentationInfo&,
                                                PresentationRequest*);
  static ControllerPresentationConnection* Take(PresentationController*,
                                                const WebPresentationInfo&,
                                                PresentationRequest*);

  ControllerPresentationConnection(LocalFrame&,
                                   PresentationController*,
                                   const String& id,
                                   const KURL&);
  ~ControllerPresentationConnection() override;

  virtual void Trace(blink::Visitor*);

  // WebPresentationConnection implementation.
  void Init() override;

 private:
  // PresentationConnection implementation.
  void DoClose() override;
  void DoTerminate() override;

  Member<PresentationController> controller_;
};

// Represents the receiver side connection of a 1-UA presentation. Instances of
// this class are created as a result of
// PresentationReceiver::OnReceiverConnectionAvailable, which in turn is a
// result of creating the controller side connection of a 1-UA presentation.
class ReceiverPresentationConnection final : public PresentationConnection {
 public:
  static ReceiverPresentationConnection* Take(
      PresentationReceiver*,
      const mojom::blink::PresentationInfo&,
      mojom::blink::PresentationConnectionPtr controller_connection,
      mojom::blink::PresentationConnectionRequest receiver_connection_request);

  ReceiverPresentationConnection(LocalFrame&,
                                 PresentationReceiver*,
                                 const String& id,
                                 const KURL&);
  ~ReceiverPresentationConnection() override;

  virtual void Trace(blink::Visitor*);

  void Init(
      mojom::blink::PresentationConnectionPtr controller_connection_ptr,
      mojom::blink::PresentationConnectionRequest receiver_connection_request);

  // PresentationConnection override
  void DidChangeState(mojom::blink::PresentationConnectionState) override;

  // Changes |state_| to TERMINATED and notifies |target_connection_|.
  void OnReceiverTerminated();

 private:
  // PresentationConnection implementation.
  void DoClose() override;
  void DoTerminate() override;

  Member<PresentationReceiver> receiver_;
};

}  // namespace blink

#endif  // PresentationConnection_h
