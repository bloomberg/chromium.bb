// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PresentationConnection_h
#define PresentationConnection_h

#include "core/dom/ContextLifecycleObserver.h"
#include "core/events/EventTarget.h"
#include "core/fileapi/Blob.h"
#include "core/fileapi/FileError.h"
#include "platform/heap/Handle.h"
#include "platform/weborigin/KURL.h"
#include "public/platform/modules/presentation/WebPresentationConnection.h"
#include "public/platform/modules/presentation/WebPresentationConnectionProxy.h"
#include "public/platform/modules/presentation/WebPresentationController.h"
#include "public/platform/modules/presentation/WebPresentationSessionInfo.h"
#include "wtf/text/WTFString.h"
#include <memory>

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
  static PresentationConnection* take(ScriptPromiseResolver*,
                                      const WebPresentationSessionInfo&,
                                      PresentationRequest*);
  static PresentationConnection* take(PresentationController*,
                                      const WebPresentationSessionInfo&,
                                      PresentationRequest*);
  static PresentationConnection* take(PresentationReceiver*,
                                      const WebPresentationSessionInfo&);
  ~PresentationConnection() override;

  // EventTarget implementation.
  const AtomicString& interfaceName() const override;
  ExecutionContext* getExecutionContext() const override;

  DECLARE_VIRTUAL_TRACE();

  const String& id() const { return m_id; }
  const String& url() const { return m_url; }
  const WTF::AtomicString& state() const;

  void send(const String& message, ExceptionState&);
  void send(DOMArrayBuffer*, ExceptionState&);
  void send(DOMArrayBufferView*, ExceptionState&);
  void send(Blob*, ExceptionState&);
  void close();
  void terminate();

  String binaryType() const;
  void setBinaryType(const String&);

  DEFINE_ATTRIBUTE_EVENT_LISTENER(message);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(connect);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(close);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(terminate);

  // Returns true if and only if the the session info represents this
  // connection.
  bool matches(const WebPresentationSessionInfo&) const;

  // Returns true if this connection's id equals to |id| and its url equals to
  // |url|.
  bool matches(const String& id, const KURL&) const;

  // Notifies the connection about its state change to 'closed'.
  void didClose(WebPresentationConnectionCloseReason, const String& message);

  // WebPresentationConnection implementation.
  void bindProxy(std::unique_ptr<WebPresentationConnectionProxy>) override;
  void didReceiveTextMessage(const WebString& message) override;
  void didReceiveBinaryMessage(const uint8_t* data, size_t length) override;
  void didChangeState(WebPresentationConnectionState) override;

  WebPresentationConnectionState getState();

 protected:
  // EventTarget implementation.
  void addedEventListener(const AtomicString& eventType,
                          RegisteredEventListener&) override;

 private:
  class BlobLoader;

  enum MessageType {
    MessageTypeText,
    MessageTypeArrayBuffer,
    MessageTypeBlob,
  };

  enum BinaryType { BinaryTypeBlob, BinaryTypeArrayBuffer };

  class Message;

  PresentationConnection(LocalFrame*, const String& id, const KURL&);

  bool canSendMessage(ExceptionState&);
  void handleMessageQueue();

  // Callbacks invoked from BlobLoader.
  void didFinishLoadingBlob(DOMArrayBuffer*);
  void didFailLoadingBlob(FileError::ErrorCode);

  // Internal helper function to dispatch state change events asynchronously.
  void dispatchStateChangeEvent(Event*);
  static void dispatchEventAsync(EventTarget*, Event*);

  // Cancel loads and pending messages when the connection is closed.
  void tearDown();

  String m_id;
  KURL m_url;
  WebPresentationConnectionState m_state;

  // For Blob data handling.
  Member<BlobLoader> m_blobLoader;
  HeapDeque<Member<Message>> m_messages;

  BinaryType m_binaryType;

  std::unique_ptr<WebPresentationConnectionProxy> m_proxy;
};

}  // namespace blink

#endif  // PresentationConnection_h
