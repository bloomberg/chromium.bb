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
#include "platform/heap/Handle.h"
#include "platform/weborigin/KURL.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/modules/presentation/WebPresentationConnection.h"
#include "public/platform/modules/presentation/WebPresentationConnectionProxy.h"
#include "public/platform/modules/presentation/WebPresentationController.h"
#include "public/platform/modules/presentation/WebPresentationInfo.h"

namespace WTF {
class AtomicString;
}  // namespace WTF

namespace blink {

class DOMArrayBuffer;
class DOMArrayBufferView;
class PresentationController;
class PresentationReceiver;
class PresentationRequest;

class PresentationConnection final : public EventTargetWithInlineData,
                                     public ContextClient,
                                     public WebPresentationConnection {
  USING_GARBAGE_COLLECTED_MIXIN(PresentationConnection);
  DEFINE_WRAPPERTYPEINFO();

 public:
  // For CallbackPromiseAdapter.
  static PresentationConnection* Take(ScriptPromiseResolver*,
                                      const WebPresentationInfo&,
                                      PresentationRequest*);
  static PresentationConnection* Take(PresentationController*,
                                      const WebPresentationInfo&,
                                      PresentationRequest*);
  static PresentationConnection* Take(PresentationReceiver*,
                                      const WebPresentationInfo&);
  ~PresentationConnection() override;

  // EventTarget implementation.
  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;

  DECLARE_VIRTUAL_TRACE();

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

  // WebPresentationConnection implementation.
  void BindProxy(std::unique_ptr<WebPresentationConnectionProxy>) override;
  void DidReceiveTextMessage(const WebString& message) override;
  void DidReceiveBinaryMessage(const uint8_t* data, size_t length) override;
  void DidChangeState(WebPresentationConnectionState) override;
  void DidClose() override;

  WebPresentationConnectionState GetState();
  void DidChangeState(WebPresentationConnectionState,
                      bool should_dispatch_event);
  // Notify target connection about connection state change.
  void NotifyTargetConnection(WebPresentationConnectionState);

 protected:
  // EventTarget implementation.
  void AddedEventListener(const AtomicString& event_type,
                          RegisteredEventListener&) override;

 private:
  class BlobLoader;

  enum MessageType {
    kMessageTypeText,
    kMessageTypeArrayBuffer,
    kMessageTypeBlob,
  };

  enum BinaryType { kBinaryTypeBlob, kBinaryTypeArrayBuffer };

  class Message;

  PresentationConnection(LocalFrame*, const String& id, const KURL&);

  bool CanSendMessage(ExceptionState&);
  void HandleMessageQueue();

  // Callbacks invoked from BlobLoader.
  void DidFinishLoadingBlob(DOMArrayBuffer*);
  void DidFailLoadingBlob(FileError::ErrorCode);

  // Internal helper function to dispatch state change events asynchronously.
  void DispatchStateChangeEvent(Event*);
  static void DispatchEventAsync(EventTarget*, Event*);

  // Cancel loads and pending messages when the connection is closed.
  void TearDown();

  String id_;
  KURL url_;
  WebPresentationConnectionState state_;

  // For Blob data handling.
  Member<BlobLoader> blob_loader_;
  HeapDeque<Member<Message>> messages_;

  BinaryType binary_type_;

  std::unique_ptr<WebPresentationConnectionProxy> proxy_;
};

}  // namespace blink

#endif  // PresentationConnection_h
