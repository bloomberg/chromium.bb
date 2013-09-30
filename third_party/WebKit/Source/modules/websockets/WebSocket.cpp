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

#include "config.h"

#include "modules/websockets/WebSocket.h"

#include "bindings/v8/ExceptionMessages.h"
#include "bindings/v8/ExceptionState.h"
#include "bindings/v8/ScriptController.h"
#include "core/dom/Document.h"
#include "core/events/Event.h"
#include "core/events/EventListener.h"
#include "core/events/EventNames.h"
#include "core/dom/ExceptionCode.h"
#include "core/events/MessageEvent.h"
#include "core/dom/ScriptExecutionContext.h"
#include "core/fileapi/Blob.h"
#include "core/inspector/ScriptCallStack.h"
#include "core/page/ConsoleTypes.h"
#include "core/page/ContentSecurityPolicy.h"
#include "core/page/DOMWindow.h"
#include "core/page/Frame.h"
#include "platform/Logging.h"
#include "core/platform/network/BlobData.h"
#include "modules/websockets/CloseEvent.h"
#include "modules/websockets/WebSocketChannel.h"
#include "weborigin/KnownPorts.h"
#include "weborigin/SecurityOrigin.h"
#include "wtf/ArrayBuffer.h"
#include "wtf/ArrayBufferView.h"
#include "wtf/HashSet.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/StdLibExtras.h"
#include "wtf/text/CString.h"
#include "wtf/text/StringBuilder.h"
#include "wtf/text/WTFString.h"

using namespace std;

namespace WebCore {

const size_t maxReasonSizeInBytes = 123;

static inline bool isValidProtocolCharacter(UChar character)
{
    // Hybi-10 says "(Subprotocol string must consist of) characters in the range U+0021 to U+007E not including
    // separator characters as defined in [RFC2616]."
    const UChar minimumProtocolCharacter = '!'; // U+0021.
    const UChar maximumProtocolCharacter = '~'; // U+007E.
    return character >= minimumProtocolCharacter && character <= maximumProtocolCharacter
        && character != '"' && character != '(' && character != ')' && character != ',' && character != '/'
        && !(character >= ':' && character <= '@') // U+003A - U+0040 (':', ';', '<', '=', '>', '?', '@').
        && !(character >= '[' && character <= ']') // U+005B - U+005D ('[', '\\', ']').
        && character != '{' && character != '}';
}

static bool isValidProtocolString(const String& protocol)
{
    if (protocol.isEmpty())
        return false;
    for (size_t i = 0; i < protocol.length(); ++i) {
        if (!isValidProtocolCharacter(protocol[i]))
            return false;
    }
    return true;
}

static String encodeProtocolString(const String& protocol)
{
    StringBuilder builder;
    for (size_t i = 0; i < protocol.length(); i++) {
        if (protocol[i] < 0x20 || protocol[i] > 0x7E)
            builder.append(String::format("\\u%04X", protocol[i]));
        else if (protocol[i] == 0x5c)
            builder.append("\\\\");
        else
            builder.append(protocol[i]);
    }
    return builder.toString();
}

static String joinStrings(const Vector<String>& strings, const char* separator)
{
    StringBuilder builder;
    for (size_t i = 0; i < strings.size(); ++i) {
        if (i)
            builder.append(separator);
        builder.append(strings[i]);
    }
    return builder.toString();
}

static unsigned long saturateAdd(unsigned long a, unsigned long b)
{
    if (numeric_limits<unsigned long>::max() - a < b)
        return numeric_limits<unsigned long>::max();
    return a + b;
}

const char* WebSocket::subProtocolSeperator()
{
    return ", ";
}

WebSocket::WebSocket(ScriptExecutionContext* context)
    : ActiveDOMObject(context)
    , m_state(CONNECTING)
    , m_bufferedAmount(0)
    , m_bufferedAmountAfterClose(0)
    , m_binaryType(BinaryTypeBlob)
    , m_subprotocol("")
    , m_extensions("")
{
    ScriptWrappable::init(this);
}

WebSocket::~WebSocket()
{
    if (m_channel)
        m_channel->disconnect();
}

PassRefPtr<WebSocket> WebSocket::create(ScriptExecutionContext* context)
{
    RefPtr<WebSocket> webSocket(adoptRef(new WebSocket(context)));
    webSocket->suspendIfNeeded();
    return webSocket.release();
}

PassRefPtr<WebSocket> WebSocket::create(ScriptExecutionContext* context, const String& url, ExceptionState& es)
{
    Vector<String> protocols;
    return WebSocket::create(context, url, protocols, es);
}

PassRefPtr<WebSocket> WebSocket::create(ScriptExecutionContext* context, const String& url, const Vector<String>& protocols, ExceptionState& es)
{
    if (url.isNull()) {
        es.throwDOMException(SyntaxError, "Failed to create a WebSocket: the provided URL is invalid.");
        return 0;
    }

    RefPtr<WebSocket> webSocket(adoptRef(new WebSocket(context)));
    webSocket->suspendIfNeeded();

    webSocket->connect(context->completeURL(url), protocols, es);
    if (es.hadException())
        return 0;

    return webSocket.release();
}

PassRefPtr<WebSocket> WebSocket::create(ScriptExecutionContext* context, const String& url, const String& protocol, ExceptionState& es)
{
    Vector<String> protocols;
    protocols.append(protocol);
    return WebSocket::create(context, url, protocols, es);
}

void WebSocket::connect(const String& url, ExceptionState& es)
{
    Vector<String> protocols;
    connect(url, protocols, es);
}

void WebSocket::connect(const String& url, const String& protocol, ExceptionState& es)
{
    Vector<String> protocols;
    protocols.append(protocol);
    connect(url, protocols, es);
}

void WebSocket::connect(const String& url, const Vector<String>& protocols, ExceptionState& es)
{
    LOG(Network, "WebSocket %p connect() url='%s'", this, url.utf8().data());
    m_url = KURL(KURL(), url);

    if (!m_url.isValid()) {
        m_state = CLOSED;
        es.throwDOMException(SyntaxError, ExceptionMessages::failedToExecute("connect", "WebSocket", "the URL '" + url + "' is invalid."));
        return;
    }
    if (!m_url.protocolIs("ws") && !m_url.protocolIs("wss")) {
        m_state = CLOSED;
        es.throwDOMException(SyntaxError, ExceptionMessages::failedToExecute("connect", "WebSocket", "The URL's scheme must be either 'ws' or 'wss'. '" + m_url.protocol() + "' is not allowed."));
        return;
    }
    if (m_url.hasFragmentIdentifier()) {
        m_state = CLOSED;
        es.throwDOMException(SyntaxError, ExceptionMessages::failedToExecute("connect", "WebSocket", "The URL contains a fragment identifier ('" + m_url.fragmentIdentifier() + "'). Fragment identifiers are not allowed in WebSocket URLs."));
        return;
    }
    if (!portAllowed(m_url)) {
        m_state = CLOSED;
        es.throwSecurityError(ExceptionMessages::failedToExecute("connect", "WebSocket", "The port " + String::number(m_url.port()) + " is not allowed."));
        return;
    }

    // FIXME: Convert this to check the isolated world's Content Security Policy once webkit.org/b/104520 is solved.
    bool shouldBypassMainWorldContentSecurityPolicy = false;
    if (scriptExecutionContext()->isDocument()) {
        Document* document = toDocument(scriptExecutionContext());
        shouldBypassMainWorldContentSecurityPolicy = document->frame()->script()->shouldBypassMainWorldContentSecurityPolicy();
    }
    if (!shouldBypassMainWorldContentSecurityPolicy && !scriptExecutionContext()->contentSecurityPolicy()->allowConnectToSource(m_url)) {
        m_state = CLOSED;
        // The URL is safe to expose to JavaScript, as this check happens synchronously before redirection.
        es.throwSecurityError(ExceptionMessages::failedToExecute("connect", "WebSocket", "Refused to connect to '" + m_url.elidedString() + "' because it violates the document's Content Security Policy."));
        return;
    }

    m_channel = WebSocketChannel::create(scriptExecutionContext(), this);

    // FIXME: There is a disagreement about restriction of subprotocols between WebSocket API and hybi-10 protocol
    // draft. The former simply says "only characters in the range U+0021 to U+007E are allowed," while the latter
    // imposes a stricter rule: "the elements MUST be non-empty strings with characters as defined in [RFC2616],
    // and MUST all be unique strings."
    //
    // Here, we throw SyntaxError if the given protocols do not meet the latter criteria. This behavior does not
    // comply with WebSocket API specification, but it seems to be the only reasonable way to handle this conflict.
    for (size_t i = 0; i < protocols.size(); ++i) {
        if (!isValidProtocolString(protocols[i])) {
            m_state = CLOSED;
            es.throwDOMException(SyntaxError, ExceptionMessages::failedToExecute("connect", "WebSocket", "The subprotocol '" + encodeProtocolString(protocols[i]) + "' is invalid."));
            return;
        }
    }
    HashSet<String> visited;
    for (size_t i = 0; i < protocols.size(); ++i) {
        if (!visited.add(protocols[i]).isNewEntry) {
            m_state = CLOSED;
            es.throwDOMException(SyntaxError, ExceptionMessages::failedToExecute("connect", "WebSocket", "The subprotocol '" + encodeProtocolString(protocols[i]) + "' is duplicated."));
            return;
        }
    }

    String protocolString;
    if (!protocols.isEmpty())
        protocolString = joinStrings(protocols, subProtocolSeperator());

    m_channel->connect(m_url, protocolString);
    ActiveDOMObject::setPendingActivity(this);
}

void WebSocket::handleSendResult(WebSocketChannel::SendResult result, ExceptionState& es)
{
    switch (result) {
    case WebSocketChannel::InvalidMessage:
        es.throwDOMException(SyntaxError, ExceptionMessages::failedToExecute("send", "WebSocket", "the message contains invalid characters."));
        return;
    case WebSocketChannel::SendFail:
        scriptExecutionContext()->addConsoleMessage(JSMessageSource, ErrorMessageLevel, "WebSocket send() failed.");
        return;
    case WebSocketChannel::SendSuccess:
        return;
    }
    ASSERT_NOT_REACHED();
}

void WebSocket::updateBufferedAmountAfterClose(unsigned long payloadSize)
{
    m_bufferedAmountAfterClose = saturateAdd(m_bufferedAmountAfterClose, payloadSize);
    m_bufferedAmountAfterClose = saturateAdd(m_bufferedAmountAfterClose, getFramingOverhead(payloadSize));

    scriptExecutionContext()->addConsoleMessage(JSMessageSource, ErrorMessageLevel, "WebSocket is already in CLOSING or CLOSED state.");
}

void WebSocket::send(const String& message, ExceptionState& es)
{
    LOG(Network, "WebSocket %p send() Sending String '%s'", this, message.utf8().data());
    if (m_state == CONNECTING) {
        es.throwDOMException(InvalidStateError, ExceptionMessages::failedToExecute("send", "WebSocket", "already in CONNECTING state."));
        return;
    }
    // No exception is raised if the connection was once established but has subsequently been closed.
    if (m_state == CLOSING || m_state == CLOSED) {
        updateBufferedAmountAfterClose(message.utf8().length());
        return;
    }
    ASSERT(m_channel);
    handleSendResult(m_channel->send(message), es);
}

void WebSocket::send(ArrayBuffer* binaryData, ExceptionState& es)
{
    LOG(Network, "WebSocket %p send() Sending ArrayBuffer %p", this, binaryData);
    ASSERT(binaryData);
    if (m_state == CONNECTING) {
        es.throwDOMException(InvalidStateError, ExceptionMessages::failedToExecute("send", "WebSocket", "already in CONNECTING state."));
        return;
    }
    if (m_state == CLOSING || m_state == CLOSED) {
        updateBufferedAmountAfterClose(binaryData->byteLength());
        return;
    }
    ASSERT(m_channel);
    handleSendResult(m_channel->send(*binaryData, 0, binaryData->byteLength()), es);
}

void WebSocket::send(ArrayBufferView* arrayBufferView, ExceptionState& es)
{
    LOG(Network, "WebSocket %p send() Sending ArrayBufferView %p", this, arrayBufferView);
    ASSERT(arrayBufferView);
    if (m_state == CONNECTING) {
        es.throwDOMException(InvalidStateError, ExceptionMessages::failedToExecute("send", "WebSocket", "already in CONNECTING state."));
        return;
    }
    if (m_state == CLOSING || m_state == CLOSED) {
        updateBufferedAmountAfterClose(arrayBufferView->byteLength());
        return;
    }
    ASSERT(m_channel);
    RefPtr<ArrayBuffer> arrayBuffer(arrayBufferView->buffer());
    handleSendResult(m_channel->send(*arrayBuffer, arrayBufferView->byteOffset(), arrayBufferView->byteLength()), es);
}

void WebSocket::send(Blob* binaryData, ExceptionState& es)
{
    LOG(Network, "WebSocket %p send() Sending Blob '%s'", this, binaryData->url().elidedString().utf8().data());
    ASSERT(binaryData);
    if (m_state == CONNECTING) {
        es.throwDOMException(InvalidStateError, ExceptionMessages::failedToExecute("send", "WebSocket", "already in CONNECTING state."));
        return;
    }
    if (m_state == CLOSING || m_state == CLOSED) {
        updateBufferedAmountAfterClose(static_cast<unsigned long>(binaryData->size()));
        return;
    }
    ASSERT(m_channel);
    handleSendResult(m_channel->send(*binaryData), es);
}

void WebSocket::close(unsigned short code, const String& reason, ExceptionState& es)
{
    closeInternal(code, reason, es);
}

void WebSocket::close(ExceptionState& es)
{
    closeInternal(WebSocketChannel::CloseEventCodeNotSpecified, String(), es);
}

void WebSocket::close(unsigned short code, ExceptionState& es)
{
    closeInternal(code, String(), es);
}

void WebSocket::closeInternal(int code, const String& reason, ExceptionState& es)
{
    if (code == WebSocketChannel::CloseEventCodeNotSpecified)
        LOG(Network, "WebSocket %p close() without code and reason", this);
    else {
        LOG(Network, "WebSocket %p close() code=%d reason='%s'", this, code, reason.utf8().data());
        if (!(code == WebSocketChannel::CloseEventCodeNormalClosure || (WebSocketChannel::CloseEventCodeMinimumUserDefined <= code && code <= WebSocketChannel::CloseEventCodeMaximumUserDefined))) {
            es.throwDOMException(InvalidAccessError, ExceptionMessages::failedToExecute("close", "WebSocket", "the code must be either 1000, or between 3000 and 4999. " + String::number(code) + " is neither."));
            return;
        }
        CString utf8 = reason.utf8(String::StrictConversionReplacingUnpairedSurrogatesWithFFFD);
        if (utf8.length() > maxReasonSizeInBytes) {
            es.throwDOMException(SyntaxError, ExceptionMessages::failedToExecute("close", "WebSocket", "the message must be smaller than " + String::number(maxReasonSizeInBytes) + " bytes."));
            return;
        }
    }

    if (m_state == CLOSING || m_state == CLOSED)
        return;
    if (m_state == CONNECTING) {
        m_state = CLOSING;
        m_channel->fail("WebSocket is closed before the connection is established.", WarningMessageLevel);
        return;
    }
    m_state = CLOSING;
    if (m_channel)
        m_channel->close(code, reason);
}

const KURL& WebSocket::url() const
{
    return m_url;
}

WebSocket::State WebSocket::readyState() const
{
    return m_state;
}

unsigned long WebSocket::bufferedAmount() const
{
    return saturateAdd(m_bufferedAmount, m_bufferedAmountAfterClose);
}

String WebSocket::protocol() const
{
    return m_subprotocol;
}

String WebSocket::extensions() const
{
    return m_extensions;
}

String WebSocket::binaryType() const
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

void WebSocket::setBinaryType(const String& binaryType)
{
    if (binaryType == "blob") {
        m_binaryType = BinaryTypeBlob;
        return;
    }
    if (binaryType == "arraybuffer") {
        m_binaryType = BinaryTypeArrayBuffer;
        return;
    }
    scriptExecutionContext()->addConsoleMessage(JSMessageSource, ErrorMessageLevel, "'" + binaryType + "' is not a valid value for binaryType; binaryType remains unchanged.");
}

const AtomicString& WebSocket::interfaceName() const
{
    return eventNames().interfaceForWebSocket;
}

ScriptExecutionContext* WebSocket::scriptExecutionContext() const
{
    return ActiveDOMObject::scriptExecutionContext();
}

void WebSocket::contextDestroyed()
{
    LOG(Network, "WebSocket %p contextDestroyed()", this);
    ASSERT(!m_channel);
    ASSERT(m_state == CLOSED);
    ActiveDOMObject::contextDestroyed();
}

bool WebSocket::canSuspend() const
{
    return !m_channel;
}

void WebSocket::suspend(ReasonForSuspension)
{
    if (m_channel)
        m_channel->suspend();
}

void WebSocket::resume()
{
    if (m_channel)
        m_channel->resume();
}

void WebSocket::stop()
{
    if (!hasPendingActivity()) {
        ASSERT(!m_channel);
        ASSERT(m_state == CLOSED);
        return;
    }
    if (m_channel) {
        m_channel->close(WebSocketChannel::CloseEventCodeGoingAway, String());
        m_channel->disconnect();
        m_channel = 0;
    }
    m_state = CLOSED;
    ActiveDOMObject::stop();
    ActiveDOMObject::unsetPendingActivity(this);
}

void WebSocket::didConnect()
{
    LOG(Network, "WebSocket %p didConnect()", this);
    if (m_state != CONNECTING)
        return;
    ASSERT(scriptExecutionContext());
    m_state = OPEN;
    m_subprotocol = m_channel->subprotocol();
    m_extensions = m_channel->extensions();
    dispatchEvent(Event::create(eventNames().openEvent));
}

void WebSocket::didReceiveMessage(const String& msg)
{
    LOG(Network, "WebSocket %p didReceiveMessage() Text message '%s'", this, msg.utf8().data());
    if (m_state != OPEN)
        return;
    ASSERT(scriptExecutionContext());
    dispatchEvent(MessageEvent::create(msg, SecurityOrigin::create(m_url)->toString()));
}

void WebSocket::didReceiveBinaryData(PassOwnPtr<Vector<char> > binaryData)
{
    LOG(Network, "WebSocket %p didReceiveBinaryData() %lu byte binary message", this, static_cast<unsigned long>(binaryData->size()));
    switch (m_binaryType) {
    case BinaryTypeBlob: {
        size_t size = binaryData->size();
        RefPtr<RawData> rawData = RawData::create();
        binaryData->swap(*rawData->mutableData());
        OwnPtr<BlobData> blobData = BlobData::create();
        blobData->appendData(rawData.release(), 0, BlobDataItem::toEndOfFile);
        RefPtr<Blob> blob = Blob::create(blobData.release(), size);
        dispatchEvent(MessageEvent::create(blob.release(), SecurityOrigin::create(m_url)->toString()));
        break;
    }

    case BinaryTypeArrayBuffer:
        dispatchEvent(MessageEvent::create(ArrayBuffer::create(binaryData->data(), binaryData->size()), SecurityOrigin::create(m_url)->toString()));
        break;
    }
}

void WebSocket::didReceiveMessageError()
{
    LOG(Network, "WebSocket %p didReceiveMessageError()", this);
    ASSERT(scriptExecutionContext());
    dispatchEvent(Event::create(eventNames().errorEvent));
}

void WebSocket::didUpdateBufferedAmount(unsigned long bufferedAmount)
{
    LOG(Network, "WebSocket %p didUpdateBufferedAmount() New bufferedAmount is %lu", this, bufferedAmount);
    if (m_state == CLOSED)
        return;
    m_bufferedAmount = bufferedAmount;
}

void WebSocket::didStartClosingHandshake()
{
    LOG(Network, "WebSocket %p didStartClosingHandshake()", this);
    m_state = CLOSING;
}

void WebSocket::didClose(unsigned long unhandledBufferedAmount, ClosingHandshakeCompletionStatus closingHandshakeCompletion, unsigned short code, const String& reason)
{
    LOG(Network, "WebSocket %p didClose()", this);
    if (!m_channel)
        return;
    bool wasClean = m_state == CLOSING && !unhandledBufferedAmount && closingHandshakeCompletion == ClosingHandshakeComplete && code != WebSocketChannel::CloseEventCodeAbnormalClosure;
    m_state = CLOSED;
    m_bufferedAmount = unhandledBufferedAmount;
    ASSERT(scriptExecutionContext());
    RefPtr<CloseEvent> event = CloseEvent::create(wasClean, code, reason);
    dispatchEvent(event);
    if (m_channel) {
        m_channel->disconnect();
        m_channel = 0;
    }
    if (hasPendingActivity())
        ActiveDOMObject::unsetPendingActivity(this);
}

EventTargetData* WebSocket::eventTargetData()
{
    return &m_eventTargetData;
}

EventTargetData* WebSocket::ensureEventTargetData()
{
    return &m_eventTargetData;
}

size_t WebSocket::getFramingOverhead(size_t payloadSize)
{
    static const size_t hybiBaseFramingOverhead = 2; // Every frame has at least two-byte header.
    static const size_t hybiMaskingKeyLength = 4; // Every frame from client must have masking key.
    static const size_t minimumPayloadSizeWithTwoByteExtendedPayloadLength = 126;
    static const size_t minimumPayloadSizeWithEightByteExtendedPayloadLength = 0x10000;
    size_t overhead = hybiBaseFramingOverhead + hybiMaskingKeyLength;
    if (payloadSize >= minimumPayloadSizeWithEightByteExtendedPayloadLength)
        overhead += 8;
    else if (payloadSize >= minimumPayloadSizeWithTwoByteExtendedPayloadLength)
        overhead += 2;
    return overhead;
}

} // namespace WebCore
