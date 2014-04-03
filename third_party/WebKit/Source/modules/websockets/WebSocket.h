/*
 * Copyright (C) 2011 Google Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebSocket_h
#define WebSocket_h

#include "bindings/v8/ScriptWrappable.h"
#include "core/dom/ActiveDOMObject.h"
#include "core/events/EventListener.h"
#include "core/events/EventTarget.h"
#include "modules/websockets/WebSocketChannel.h"
#include "modules/websockets/WebSocketChannelClient.h"
#include "platform/Timer.h"
#include "platform/weborigin/KURL.h"
#include "wtf/Deque.h"
#include "wtf/Forward.h"
#include "wtf/OwnPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/text/AtomicStringHash.h"

namespace WebCore {

class Blob;
class ExceptionState;

class WebSocket FINAL : public RefCounted<WebSocket>, public ScriptWrappable, public EventTargetWithInlineData, public ActiveDOMObject, public WebSocketChannelClient {
    REFCOUNTED_EVENT_TARGET(WebSocket);
public:
    static const char* subProtocolSeperator();
    // WebSocket instances must be used with a wrapper since this class's
    // lifetime management is designed assuming the V8 holds a ref on it while
    // hasPendingActivity() returns true.
    static PassRefPtr<WebSocket> create(ExecutionContext*, const String& url, ExceptionState&);
    static PassRefPtr<WebSocket> create(ExecutionContext*, const String& url, const String& protocol, ExceptionState&);
    static PassRefPtr<WebSocket> create(ExecutionContext*, const String& url, const Vector<String>& protocols, ExceptionState&);
    virtual ~WebSocket();

    enum State {
        CONNECTING = 0,
        OPEN = 1,
        CLOSING = 2,
        CLOSED = 3
    };

    void connect(const String& url, ExceptionState&);
    void connect(const String& url, const String& protocol, ExceptionState&);
    void connect(const String& url, const Vector<String>& protocols, ExceptionState&);

    void send(const String& message, ExceptionState&);
    void send(ArrayBuffer*, ExceptionState&);
    void send(ArrayBufferView*, ExceptionState&);
    void send(Blob*, ExceptionState&);

    // To distinguish close method call with the code parameter from one
    // without, we have these three signatures. Use of
    // Optional=DefaultIsUndefined in the IDL file doesn't help for now since
    // it's bound to a value of 0 which is indistinguishable from the case 0
    // is passed as code parameter.
    void close(unsigned short code, const String& reason, ExceptionState&);
    void close(ExceptionState&);
    void close(unsigned short code, ExceptionState&);

    const KURL& url() const;
    State readyState() const;
    unsigned long bufferedAmount() const;

    String protocol() const;
    String extensions() const;

    String binaryType() const;
    void setBinaryType(const String&);

    DEFINE_ATTRIBUTE_EVENT_LISTENER(open);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(message);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(error);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(close);

    // EventTarget functions.
    virtual const AtomicString& interfaceName() const OVERRIDE;
    virtual ExecutionContext* executionContext() const OVERRIDE;

    // ActiveDOMObject functions.
    virtual void contextDestroyed() OVERRIDE;
    // Prevent this instance from being collected while it's not in CLOSED
    // state.
    virtual bool hasPendingActivity() const OVERRIDE;
    virtual void suspend() OVERRIDE;
    virtual void resume() OVERRIDE;
    virtual void stop() OVERRIDE;

    // WebSocketChannelClient functions.
    virtual void didConnect() OVERRIDE;
    virtual void didReceiveMessage(const String& message) OVERRIDE;
    virtual void didReceiveBinaryData(PassOwnPtr<Vector<char> >) OVERRIDE;
    virtual void didReceiveMessageError() OVERRIDE;
    virtual void didUpdateBufferedAmount(unsigned long bufferedAmount) OVERRIDE;
    virtual void didStartClosingHandshake() OVERRIDE;
    virtual void didClose(unsigned long unhandledBufferedAmount, ClosingHandshakeCompletionStatus, unsigned short code, const String& reason) OVERRIDE;

private:
    // FIXME: This should inherit WebCore::EventQueue.
    class EventQueue FINAL : public RefCounted<EventQueue> {
    public:
        static PassRefPtr<EventQueue> create(EventTarget* target) { return adoptRef(new EventQueue(target)); }
        ~EventQueue();

        // Dispatches the event if this queue is active.
        // Queues the event if this queue is suspended.
        // Does nothing otherwise.
        void dispatch(PassRefPtrWillBeRawPtr<Event> /* event */);

        bool isEmpty() const;

        void suspend();
        void resume();
        void stop();

    private:
        enum State {
            Active,
            Suspended,
            Stopped,
        };

        explicit EventQueue(EventTarget*);

        // Dispatches queued events if this queue is active.
        // Does nothing otherwise.
        void dispatchQueuedEvents();
        void resumeTimerFired(Timer<EventQueue>*);

        State m_state;
        EventTarget* m_target;
        // FIXME: oilpan: This should be HeapDeque once it's implemented.
        Deque<RefPtrWillBePersistent<Event> > m_events;
        Timer<EventQueue> m_resumeTimer;
    };

    explicit WebSocket(ExecutionContext*);

    // Adds a console message with JSMessageSource and ErrorMessageLevel.
    void logError(const String& message);

    // Handle the JavaScript close method call. close() methods on this class
    // are just for determining if the optional code argument is supplied or
    // not.
    void closeInternal(int, const String&, ExceptionState&);

    size_t getFramingOverhead(size_t payloadSize);

    // Checks the result of WebSocketChannel::send() method, and shows console
    // message and sets ec appropriately.
    void handleSendResult(WebSocketChannel::SendResult, ExceptionState&);

    // Updates m_bufferedAmountAfterClose given the amount of data passed to
    // send() method after the state changed to CLOSING or CLOSED.
    void updateBufferedAmountAfterClose(unsigned long);

    void releaseChannel();

    enum BinaryType {
        BinaryTypeBlob,
        BinaryTypeArrayBuffer
    };

    RefPtr<WebSocketChannel> m_channel;

    State m_state;
    KURL m_url;
    unsigned long m_bufferedAmount;
    unsigned long m_bufferedAmountAfterClose;
    BinaryType m_binaryType;
    String m_subprotocol;
    String m_extensions;

    RefPtr<EventQueue> m_eventQueue;
};

} // namespace WebCore

#endif // WebSocket_h
