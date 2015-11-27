// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PresentationConnection_h
#define PresentationConnection_h

#include "core/events/EventTarget.h"
#include "core/fileapi/Blob.h"
#include "core/fileapi/FileError.h"
#include "core/frame/DOMWindowProperty.h"
#include "platform/heap/Handle.h"
#include "public/platform/modules/presentation/WebPresentationConnectionClient.h"
#include "wtf/OwnPtr.h"
#include "wtf/text/WTFString.h"

namespace WTF {
class AtomicString;
} // namespace WTF

namespace blink {

class DOMArrayBuffer;
class DOMArrayBufferView;
class PresentationController;
class PresentationRequest;

class PresentationConnection final
    : public RefCountedGarbageCollectedEventTargetWithInlineData<PresentationConnection>
    , public DOMWindowProperty {
    REFCOUNTED_GARBAGE_COLLECTED_EVENT_TARGET(PresentationConnection);
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(PresentationConnection);
    DEFINE_WRAPPERTYPEINFO();
public:
    // For CallbackPromiseAdapter.
    using WebType = OwnPtr<WebPresentationConnectionClient>;

    static PresentationConnection* take(ScriptPromiseResolver*, PassOwnPtr<WebPresentationConnectionClient>, PresentationRequest*);
    static PresentationConnection* take(PresentationController*, PassOwnPtr<WebPresentationConnectionClient>, PresentationRequest*);
    ~PresentationConnection() override;

    // EventTarget implementation.
    const AtomicString& interfaceName() const override;
    ExecutionContext* executionContext() const override;

    DECLARE_VIRTUAL_TRACE();

    const String& id() const { return m_id; }
    const WTF::AtomicString& state() const;

    void send(const String& message, ExceptionState&);
    void send(PassRefPtr<DOMArrayBuffer>, ExceptionState&);
    void send(PassRefPtr<DOMArrayBufferView>, ExceptionState&);
    void send(Blob*, ExceptionState&);
    void close();
    void terminate();

    String binaryType() const;
    void setBinaryType(const String&);

    DEFINE_ATTRIBUTE_EVENT_LISTENER(message);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(statechange);

    // Returns true if and only if the WebPresentationConnectionClient represents this connection.
    bool matches(WebPresentationConnectionClient*) const;

    // Notifies the connection about its state change.
    void didChangeState(WebPresentationConnectionState);

    // Notifies the presentation about new message.
    void didReceiveTextMessage(const String& message);
    void didReceiveBinaryMessage(const uint8_t* data, size_t length);

protected:
    // EventTarget implementation.
    bool addEventListenerInternal(const AtomicString& eventType, PassRefPtrWillBeRawPtr<EventListener>, const EventListenerOptions&) override;

private:
    class BlobLoader;

    enum MessageType {
        MessageTypeText,
        MessageTypeArrayBuffer,
        MessageTypeBlob,
    };

    enum BinaryType {
        BinaryTypeBlob,
        BinaryTypeArrayBuffer
    };

    struct Message {
        Message(const String& text)
            : type(MessageTypeText)
            , text(text) { }

        Message(PassRefPtr<DOMArrayBuffer> arrayBuffer)
            : type(MessageTypeArrayBuffer)
            , arrayBuffer(arrayBuffer) { }

        Message(PassRefPtr<BlobDataHandle> blobDataHandle)
            : type(MessageTypeBlob)
            , blobDataHandle(blobDataHandle) { }

        MessageType type;
        String text;
        RefPtr<DOMArrayBuffer> arrayBuffer;
        RefPtr<BlobDataHandle> blobDataHandle;
    };

    PresentationConnection(LocalFrame*, const String& id, const String& url);

    bool canSendMessage(ExceptionState&);
    void handleMessageQueue();

    // Callbacks invoked from BlobLoader.
    void didFinishLoadingBlob(PassRefPtr<DOMArrayBuffer>);
    void didFailLoadingBlob(FileError::ErrorCode);

    // Cancel loads and pending messages when the connection is closed.
    void tearDown();

    String m_id;
    String m_url;
    WebPresentationConnectionState m_state;

    // For Blob data handling.
    Member<BlobLoader> m_blobLoader;
    Deque<OwnPtr<Message>> m_messages;

    BinaryType m_binaryType;
};

} // namespace blink

#endif // PresentationConnection_h
