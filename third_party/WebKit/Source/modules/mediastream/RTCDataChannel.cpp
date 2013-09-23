/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "modules/mediastream/RTCDataChannel.h"

#include "bindings/v8/ExceptionState.h"
#include "core/events/Event.h"
#include "core/dom/ExceptionCode.h"
#include "core/events/MessageEvent.h"
#include "core/dom/ScriptExecutionContext.h"
#include "core/fileapi/Blob.h"
#include "core/platform/mediastream/RTCDataChannelHandler.h"
#include "core/platform/mediastream/RTCPeerConnectionHandler.h"
#include "wtf/ArrayBuffer.h"
#include "wtf/ArrayBufferView.h"

namespace WebCore {

PassRefPtr<RTCDataChannel> RTCDataChannel::create(ScriptExecutionContext* context, PassOwnPtr<RTCDataChannelHandler> handler)
{
    ASSERT(handler);
    return adoptRef(new RTCDataChannel(context, handler));
}

PassRefPtr<RTCDataChannel> RTCDataChannel::create(ScriptExecutionContext* context, RTCPeerConnectionHandler* peerConnectionHandler, const String& label, const WebKit::WebRTCDataChannelInit& init, ExceptionState& es)
{
    OwnPtr<RTCDataChannelHandler> handler = peerConnectionHandler->createDataChannel(label, init);
    if (!handler) {
        es.throwDOMException(NotSupportedError);
        return 0;
    }
    return adoptRef(new RTCDataChannel(context, handler.release()));
}

RTCDataChannel::RTCDataChannel(ScriptExecutionContext* context, PassOwnPtr<RTCDataChannelHandler> handler)
    : m_scriptExecutionContext(context)
    , m_handler(handler)
    , m_stopped(false)
    , m_readyState(ReadyStateConnecting)
    , m_binaryType(BinaryTypeArrayBuffer)
    , m_scheduledEventTimer(this, &RTCDataChannel::scheduledEventTimerFired)
{
    ScriptWrappable::init(this);
    m_handler->setClient(this);
}

RTCDataChannel::~RTCDataChannel()
{
}

String RTCDataChannel::label() const
{
    return m_handler->label();
}

bool RTCDataChannel::reliable() const
{
    return m_handler->isReliable();
}

bool RTCDataChannel::ordered() const
{
    return m_handler->ordered();
}

unsigned short RTCDataChannel::maxRetransmitTime() const
{
    return m_handler->maxRetransmitTime();
}

unsigned short RTCDataChannel::maxRetransmits() const
{
    return m_handler->maxRetransmits();
}

String RTCDataChannel::protocol() const
{
    return m_handler->protocol();
}

bool RTCDataChannel::negotiated() const
{
    return m_handler->negotiated();
}

unsigned short RTCDataChannel::id() const
{
    return m_handler->id();
}

String RTCDataChannel::readyState() const
{
    switch (m_readyState) {
    case ReadyStateConnecting:
        return "connecting";
    case ReadyStateOpen:
        return "open";
    case ReadyStateClosing:
        return "closing";
    case ReadyStateClosed:
        return "closed";
    }

    ASSERT_NOT_REACHED();
    return String();
}

unsigned long RTCDataChannel::bufferedAmount() const
{
    return m_handler->bufferedAmount();
}

String RTCDataChannel::binaryType() const
{
    switch (m_binaryType) {
    case BinaryTypeBlob:
        return "blob";
    case BinaryTypeArrayBuffer:
        return "arraybuffer";
    }
    ASSERT_NOT_REACHED();
    return String();
}

void RTCDataChannel::setBinaryType(const String& binaryType, ExceptionState& es)
{
    if (binaryType == "blob")
        es.throwDOMException(NotSupportedError);
    else if (binaryType == "arraybuffer")
        m_binaryType = BinaryTypeArrayBuffer;
    else
        es.throwDOMException(TypeMismatchError);
}

void RTCDataChannel::send(const String& data, ExceptionState& es)
{
    if (m_readyState != ReadyStateOpen) {
        es.throwDOMException(InvalidStateError);
        return;
    }
    if (!m_handler->sendStringData(data)) {
        // FIXME: Decide what the right exception here is.
        es.throwDOMException(SyntaxError);
    }
}

void RTCDataChannel::send(PassRefPtr<ArrayBuffer> prpData, ExceptionState& es)
{
    if (m_readyState != ReadyStateOpen) {
        es.throwDOMException(InvalidStateError);
        return;
    }

    RefPtr<ArrayBuffer> data = prpData;

    size_t dataLength = data->byteLength();
    if (!dataLength)
        return;

    const char* dataPointer = static_cast<const char*>(data->data());

    if (!m_handler->sendRawData(dataPointer, dataLength)) {
        // FIXME: Decide what the right exception here is.
        es.throwDOMException(SyntaxError);
    }
}

void RTCDataChannel::send(PassRefPtr<ArrayBufferView> data, ExceptionState& es)
{
    RefPtr<ArrayBuffer> arrayBuffer(data->buffer());
    send(arrayBuffer.release(), es);
}

void RTCDataChannel::send(PassRefPtr<Blob> data, ExceptionState& es)
{
    // FIXME: implement
    es.throwDOMException(NotSupportedError);
}

void RTCDataChannel::close()
{
    if (m_stopped)
        return;

    m_handler->close();
}

void RTCDataChannel::didChangeReadyState(ReadyState newState)
{
    if (m_stopped || m_readyState == ReadyStateClosed)
        return;

    m_readyState = newState;

    switch (m_readyState) {
    case ReadyStateOpen:
        scheduleDispatchEvent(Event::create(eventNames().openEvent));
        break;
    case ReadyStateClosed:
        scheduleDispatchEvent(Event::create(eventNames().closeEvent));
        break;
    default:
        break;
    }
}

void RTCDataChannel::didReceiveStringData(const String& text)
{
    if (m_stopped)
        return;

    scheduleDispatchEvent(MessageEvent::create(text));
}

void RTCDataChannel::didReceiveRawData(const char* data, size_t dataLength)
{
    if (m_stopped)
        return;

    if (m_binaryType == BinaryTypeBlob) {
        // FIXME: Implement.
        return;
    }
    if (m_binaryType == BinaryTypeArrayBuffer) {
        RefPtr<ArrayBuffer> buffer = ArrayBuffer::create(data, dataLength);
        scheduleDispatchEvent(MessageEvent::create(buffer.release()));
        return;
    }
    ASSERT_NOT_REACHED();
}

void RTCDataChannel::didDetectError()
{
    if (m_stopped)
        return;

    scheduleDispatchEvent(Event::create(eventNames().errorEvent));
}

const AtomicString& RTCDataChannel::interfaceName() const
{
    return eventNames().interfaceForRTCDataChannel;
}

ScriptExecutionContext* RTCDataChannel::scriptExecutionContext() const
{
    return m_scriptExecutionContext;
}

void RTCDataChannel::stop()
{
    m_stopped = true;
    m_readyState = ReadyStateClosed;
    m_handler->setClient(0);
    m_scriptExecutionContext = 0;
}

EventTargetData* RTCDataChannel::eventTargetData()
{
    return &m_eventTargetData;
}

EventTargetData* RTCDataChannel::ensureEventTargetData()
{
    return &m_eventTargetData;
}

void RTCDataChannel::scheduleDispatchEvent(PassRefPtr<Event> event)
{
    m_scheduledEvents.append(event);

    if (!m_scheduledEventTimer.isActive())
        m_scheduledEventTimer.startOneShot(0);
}

void RTCDataChannel::scheduledEventTimerFired(Timer<RTCDataChannel>*)
{
    if (m_stopped)
        return;

    Vector<RefPtr<Event> > events;
    events.swap(m_scheduledEvents);

    Vector<RefPtr<Event> >::iterator it = events.begin();
    for (; it != events.end(); ++it)
        dispatchEvent((*it).release());

    events.clear();
}

} // namespace WebCore
