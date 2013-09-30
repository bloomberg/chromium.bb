/*
 * Copyright (C) 2011, 2012 Google Inc.  All rights reserved.
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
#include "modules/websockets/MainThreadWebSocketChannel.h"

#include "bindings/v8/ExceptionStatePlaceholder.h"
#include "bindings/v8/ScriptCallStackFactory.h"
#include "core/dom/Document.h"
#include "core/dom/ScriptExecutionContext.h"
#include "core/fileapi/Blob.h"
#include "core/fileapi/FileError.h"
#include "core/fileapi/FileReaderLoader.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/inspector/ScriptCallStack.h"
#include "core/loader/CookieJar.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/loader/UniqueIdentifier.h"
#include "core/page/Frame.h"
#include "core/page/Page.h"
#include "core/page/Settings.h"
#include "platform/Logging.h"
#include "core/platform/network/SocketStreamError.h"
#include "core/platform/network/SocketStreamHandle.h"
#include "modules/websockets/WebSocketChannel.h"
#include "modules/websockets/WebSocketChannelClient.h"
#include "modules/websockets/WebSocketHandshake.h"
#include "wtf/ArrayBuffer.h"
#include "wtf/Deque.h"
#include "wtf/FastMalloc.h"
#include "wtf/HashMap.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/text/CString.h"
#include "wtf/text/StringHash.h"
#include "wtf/text/WTFString.h"

using namespace std;

namespace WebCore {

const double TCPMaximumSegmentLifetime = 2 * 60.0;

MainThreadWebSocketChannel::MainThreadWebSocketChannel(Document* document, WebSocketChannelClient* client, const String& sourceURL, unsigned lineNumber)
    : m_document(document)
    , m_client(client)
    , m_resumeTimer(this, &MainThreadWebSocketChannel::resumeTimerFired)
    , m_suspended(false)
    , m_didFailOfClientAlreadyRun(false)
    , m_receivedClosingHandshake(false)
    , m_closingTimer(this, &MainThreadWebSocketChannel::closingTimerFired)
    , m_state(ChannelIdle)
    , m_shouldDiscardReceivedData(false)
    , m_unhandledBufferedAmount(0)
    , m_identifier(0)
    , m_hasContinuousFrame(false)
    , m_closeEventCode(CloseEventCodeAbnormalClosure)
    , m_outgoingFrameQueueStatus(OutgoingFrameQueueOpen)
    , m_blobLoaderStatus(BlobLoaderNotStarted)
    , m_sourceURLAtConnection(sourceURL)
    , m_lineNumberAtConnection(lineNumber)
{
    if (m_document->page())
        m_identifier = createUniqueIdentifier();
}

MainThreadWebSocketChannel::~MainThreadWebSocketChannel()
{
}

void MainThreadWebSocketChannel::connect(const KURL& url, const String& protocol)
{
    LOG(Network, "MainThreadWebSocketChannel %p connect()", this);
    ASSERT(!m_handle);
    ASSERT(!m_suspended);
    m_handshake = adoptPtr(new WebSocketHandshake(url, protocol, m_document));
    m_handshake->reset();
    Settings* settings = m_document->settings();
    if (settings && settings->experimentalWebSocketEnabled())
        m_handshake->addExtensionProcessor(m_perMessageDeflate.createExtensionProcessor());
    m_handshake->addExtensionProcessor(m_deflateFramer.createExtensionProcessor());
    if (m_identifier)
        InspectorInstrumentation::didCreateWebSocket(m_document, m_identifier, url, protocol);
    ref();
    m_handle = SocketStreamHandle::create(m_handshake->url(), this);
    RefPtr<ScriptCallStack> callStack = createScriptCallStack(1, true);
    if (callStack && callStack->size()) {
        m_sourceURLAtConnection = callStack->at(0).sourceURL();
        m_lineNumberAtConnection = callStack->at(0).lineNumber();
    }
}

String MainThreadWebSocketChannel::subprotocol()
{
    LOG(Network, "MainThreadWebSocketChannel %p subprotocol()", this);
    if (!m_handshake || m_handshake->mode() != WebSocketHandshake::Connected)
        return "";
    String serverProtocol = m_handshake->serverWebSocketProtocol();
    if (serverProtocol.isNull())
        return "";
    return serverProtocol;
}

String MainThreadWebSocketChannel::extensions()
{
    LOG(Network, "MainThreadWebSocketChannel %p extensions()", this);
    if (!m_handshake || m_handshake->mode() != WebSocketHandshake::Connected)
        return "";
    String extensions = m_handshake->acceptedExtensions();
    if (extensions.isNull())
        return "";
    return extensions;
}

WebSocketChannel::SendResult MainThreadWebSocketChannel::send(const String& message)
{
    LOG(Network, "MainThreadWebSocketChannel %p send() Sending String '%s'", this, message.utf8().data());
    CString utf8 = message.utf8(String::StrictConversionReplacingUnpairedSurrogatesWithFFFD);
    enqueueTextFrame(utf8);
    processOutgoingFrameQueue();
    // m_channel->send() may happen later, thus it's not always possible to know whether
    // the message has been sent to the socket successfully. In this case, we have no choice
    // but to return SendSuccess.
    return WebSocketChannel::SendSuccess;
}

WebSocketChannel::SendResult MainThreadWebSocketChannel::send(const ArrayBuffer& binaryData, unsigned byteOffset, unsigned byteLength)
{
    LOG(Network, "MainThreadWebSocketChannel %p send() Sending ArrayBuffer %p byteOffset=%u byteLength=%u", this, &binaryData, byteOffset, byteLength);
    enqueueRawFrame(WebSocketFrame::OpCodeBinary, static_cast<const char*>(binaryData.data()) + byteOffset, byteLength);
    processOutgoingFrameQueue();
    return WebSocketChannel::SendSuccess;
}

WebSocketChannel::SendResult MainThreadWebSocketChannel::send(const Blob& binaryData)
{
    LOG(Network, "MainThreadWebSocketChannel %p send() Sending Blob '%s'", this, binaryData.url().elidedString().utf8().data());
    enqueueBlobFrame(WebSocketFrame::OpCodeBinary, binaryData);
    processOutgoingFrameQueue();
    return WebSocketChannel::SendSuccess;
}

bool MainThreadWebSocketChannel::send(const char* data, int length)
{
    LOG(Network, "MainThreadWebSocketChannel %p send() Sending char* data=%p length=%d", this, data, length);
    enqueueRawFrame(WebSocketFrame::OpCodeBinary, data, length);
    processOutgoingFrameQueue();
    return true;
}

unsigned long MainThreadWebSocketChannel::bufferedAmount() const
{
    LOG(Network, "MainThreadWebSocketChannel %p bufferedAmount()", this);
    ASSERT(m_handle);
    ASSERT(!m_suspended);
    return m_handle->bufferedAmount();
}

void MainThreadWebSocketChannel::close(int code, const String& reason)
{
    LOG(Network, "MainThreadWebSocketChannel %p close() code=%d reason='%s'", this, code, reason.utf8().data());
    ASSERT(!m_suspended);
    if (!m_handle)
        return;
    startClosingHandshake(code, reason);
    if (!m_closingTimer.isActive())
        m_closingTimer.startOneShot(2 * TCPMaximumSegmentLifetime);
}

void MainThreadWebSocketChannel::fail(const String& reason, MessageLevel level, const String& sourceURL, unsigned lineNumber)
{
    LOG(Network, "MainThreadWebSocketChannel %p fail() reason='%s'", this, reason.utf8().data());
    ASSERT(!m_suspended);
    if (m_document) {
        InspectorInstrumentation::didReceiveWebSocketFrameError(m_document, m_identifier, reason);
        const String message = "WebSocket connection to '" + m_handshake->url().elidedString() + "' failed: " + reason;
        static_cast<ScriptExecutionContext*>(m_document)->addConsoleMessage(JSMessageSource, level, message, sourceURL, lineNumber);
    }
    // Hybi-10 specification explicitly states we must not continue to handle incoming data
    // once the WebSocket connection is failed (section 7.1.7).
    RefPtr<MainThreadWebSocketChannel> protect(this); // The client can close the channel, potentially removing the last reference.
    m_shouldDiscardReceivedData = true;
    if (!m_buffer.isEmpty())
        skipBuffer(m_buffer.size()); // Save memory.
    m_deflateFramer.didFail();
    m_perMessageDeflate.didFail();
    m_hasContinuousFrame = false;
    m_continuousFrameData.clear();
    if (!m_didFailOfClientAlreadyRun) {
        m_didFailOfClientAlreadyRun = true;
        if (m_client)
            m_client->didReceiveMessageError();
    }
    if (m_handle && (m_state != ChannelClosed))
        m_handle->disconnect(); // Will call didCloseSocketStream().
}

void MainThreadWebSocketChannel::disconnect()
{
    LOG(Network, "MainThreadWebSocketChannel %p disconnect()", this);
    if (m_identifier && m_document)
        InspectorInstrumentation::didCloseWebSocket(m_document, m_identifier);
    if (m_handshake)
        m_handshake->clearScriptExecutionContext();
    m_client = 0;
    m_document = 0;
    if (m_handle)
        m_handle->disconnect();
}

void MainThreadWebSocketChannel::suspend()
{
    m_suspended = true;
}

void MainThreadWebSocketChannel::resume()
{
    m_suspended = false;
    if ((!m_buffer.isEmpty() || (m_state == ChannelClosed)) && m_client && !m_resumeTimer.isActive())
        m_resumeTimer.startOneShot(0);
}

void MainThreadWebSocketChannel::willOpenSocketStream(SocketStreamHandle* handle)
{
    LOG(Network, "MainThreadWebSocketChannel %p willOpenSocketStream()", this);
    ASSERT(handle);
    if (m_document->frame())
        m_document->frame()->loader()->client()->dispatchWillOpenSocketStream(handle);
}

void MainThreadWebSocketChannel::didOpenSocketStream(SocketStreamHandle* handle)
{
    LOG(Network, "MainThreadWebSocketChannel %p didOpenSocketStream()", this);
    ASSERT(handle == m_handle);
    if (!m_document)
        return;
    if (m_identifier)
        InspectorInstrumentation::willSendWebSocketHandshakeRequest(m_document, m_identifier, *m_handshake->clientHandshakeRequest());
    CString handshakeMessage = m_handshake->clientHandshakeMessage();
    if (!handle->send(handshakeMessage.data(), handshakeMessage.length()))
        failAsError("Failed to send WebSocket handshake.");
}

void MainThreadWebSocketChannel::didCloseSocketStream(SocketStreamHandle* handle)
{
    LOG(Network, "MainThreadWebSocketChannel %p didCloseSocketStream()", this);
    if (m_identifier && m_document)
        InspectorInstrumentation::didCloseWebSocket(m_document, m_identifier);
    ASSERT_UNUSED(handle, handle == m_handle || !m_handle);
    m_state = ChannelClosed;
    if (m_closingTimer.isActive())
        m_closingTimer.stop();
    if (m_outgoingFrameQueueStatus != OutgoingFrameQueueClosed)
        abortOutgoingFrameQueue();
    if (m_handle) {
        m_unhandledBufferedAmount = m_handle->bufferedAmount();
        if (m_suspended)
            return;
        WebSocketChannelClient* client = m_client;
        m_client = 0;
        m_document = 0;
        m_handle = 0;
        if (client)
            client->didClose(m_unhandledBufferedAmount, m_receivedClosingHandshake ? WebSocketChannelClient::ClosingHandshakeComplete : WebSocketChannelClient::ClosingHandshakeIncomplete, m_closeEventCode, m_closeEventReason);
    }
    deref();
}

void MainThreadWebSocketChannel::didReceiveSocketStreamData(SocketStreamHandle* handle, const char* data, int len)
{
    LOG(Network, "MainThreadWebSocketChannel %p didReceiveSocketStreamData() Received %d bytes", this, len);
    RefPtr<MainThreadWebSocketChannel> protect(this); // The client can close the channel, potentially removing the last reference.
    ASSERT(handle == m_handle);
    if (!m_document)
        return;
    if (len <= 0) {
        handle->disconnect();
        return;
    }
    if (!m_client) {
        m_shouldDiscardReceivedData = true;
        handle->disconnect();
        return;
    }
    if (m_shouldDiscardReceivedData)
        return;
    if (!appendToBuffer(data, len)) {
        m_shouldDiscardReceivedData = true;
        failAsError("Ran out of memory while receiving WebSocket data.");
        return;
    }
    while (!m_suspended && m_client && !m_buffer.isEmpty())
        if (!processBuffer())
            break;
}

void MainThreadWebSocketChannel::didUpdateBufferedAmount(SocketStreamHandle*, size_t bufferedAmount)
{
    if (m_client)
        m_client->didUpdateBufferedAmount(bufferedAmount);
}

void MainThreadWebSocketChannel::didFailSocketStream(SocketStreamHandle* handle, const SocketStreamError& error)
{
    LOG(Network, "MainThreadWebSocketChannel %p didFailSocketStream()", this);
    ASSERT_UNUSED(handle, handle == m_handle || !m_handle);
    m_shouldDiscardReceivedData = true;
    String message;
    if (error.isNull())
        message = "WebSocket network error";
    else if (error.localizedDescription().isNull())
        message = "WebSocket network error: error code " + String::number(error.errorCode());
    else
        message = "WebSocket network error: error code " + String::number(error.errorCode()) + ", " + error.localizedDescription();
    String failingURL = error.failingURL();
    ASSERT(failingURL.isNull() || m_handshake->url().string() == failingURL);
    if (failingURL.isNull())
        failingURL = m_handshake->url().string();
    LOG(Network, "Error Message: '%s', FailURL: '%s'", message.utf8().data(), failingURL.utf8().data());
    RefPtr<WebSocketChannel> protect(this);
    if (m_client && (m_state != ChannelClosing && m_state != ChannelClosed) && !m_didFailOfClientAlreadyRun) {
        m_didFailOfClientAlreadyRun = true;
        m_client->didReceiveMessageError();
    }
    if (m_handle && (m_state != ChannelClosed))
        m_handle->disconnect();
}

void MainThreadWebSocketChannel::didStartLoading()
{
    LOG(Network, "MainThreadWebSocketChannel %p didStartLoading()", this);
    ASSERT(m_blobLoader);
    ASSERT(m_blobLoaderStatus == BlobLoaderStarted);
}

void MainThreadWebSocketChannel::didReceiveData()
{
    LOG(Network, "MainThreadWebSocketChannel %p didReceiveData()", this);
    ASSERT(m_blobLoader);
    ASSERT(m_blobLoaderStatus == BlobLoaderStarted);
}

void MainThreadWebSocketChannel::didFinishLoading()
{
    LOG(Network, "MainThreadWebSocketChannel %p didFinishLoading()", this);
    ASSERT(m_blobLoader);
    ASSERT(m_blobLoaderStatus == BlobLoaderStarted);
    m_blobLoaderStatus = BlobLoaderFinished;
    processOutgoingFrameQueue();
    deref();
}

void MainThreadWebSocketChannel::didFail(FileError::ErrorCode errorCode)
{
    LOG(Network, "MainThreadWebSocketChannel %p didFail() errorCode=%d", this, errorCode);
    ASSERT(m_blobLoader);
    ASSERT(m_blobLoaderStatus == BlobLoaderStarted);
    m_blobLoader.clear();
    m_blobLoaderStatus = BlobLoaderFailed;
    failAsError("Failed to load Blob: error code = " + String::number(errorCode)); // FIXME: Generate human-friendly reason message.
    deref();
}

bool MainThreadWebSocketChannel::appendToBuffer(const char* data, size_t len)
{
    size_t newBufferSize = m_buffer.size() + len;
    if (newBufferSize < m_buffer.size()) {
        LOG(Network, "MainThreadWebSocketChannel %p appendToBuffer() Buffer overflow (%lu bytes already in receive buffer and appending %lu bytes)", this, static_cast<unsigned long>(m_buffer.size()), static_cast<unsigned long>(len));
        return false;
    }
    m_buffer.append(data, len);
    return true;
}

void MainThreadWebSocketChannel::skipBuffer(size_t len)
{
    ASSERT_WITH_SECURITY_IMPLICATION(len <= m_buffer.size());
    memmove(m_buffer.data(), m_buffer.data() + len, m_buffer.size() - len);
    m_buffer.resize(m_buffer.size() - len);
}

bool MainThreadWebSocketChannel::processBuffer()
{
    ASSERT(!m_suspended);
    ASSERT(m_client);
    ASSERT(!m_buffer.isEmpty());
    LOG(Network, "MainThreadWebSocketChannel %p processBuffer() Receive buffer has %lu bytes", this, static_cast<unsigned long>(m_buffer.size()));

    if (m_shouldDiscardReceivedData)
        return false;

    if (m_receivedClosingHandshake) {
        skipBuffer(m_buffer.size());
        return false;
    }

    RefPtr<MainThreadWebSocketChannel> protect(this); // The client can close the channel, potentially removing the last reference.

    if (m_handshake->mode() == WebSocketHandshake::Incomplete) {
        int headerLength = m_handshake->readServerHandshake(m_buffer.data(), m_buffer.size());
        if (headerLength <= 0)
            return false;
        if (m_handshake->mode() == WebSocketHandshake::Connected) {
            if (m_identifier)
                InspectorInstrumentation::didReceiveWebSocketHandshakeResponse(m_document, m_identifier, m_handshake->serverHandshakeResponse());
            if (!m_handshake->serverSetCookie().isEmpty()) {
                if (cookiesEnabled(m_document)) {
                    // Exception (for sandboxed documents) ignored.
                    m_document->setCookie(m_handshake->serverSetCookie(), IGNORE_EXCEPTION);
                }
            }
            // FIXME: handle set-cookie2.
            LOG(Network, "MainThreadWebSocketChannel %p Connected", this);
            skipBuffer(headerLength);
            m_client->didConnect();
            LOG(Network, "MainThreadWebSocketChannel %p %lu bytes remaining in m_buffer", this, static_cast<unsigned long>(m_buffer.size()));
            return !m_buffer.isEmpty();
        }
        ASSERT(m_handshake->mode() == WebSocketHandshake::Failed);
        LOG(Network, "MainThreadWebSocketChannel %p Connection failed", this);
        skipBuffer(headerLength);
        m_shouldDiscardReceivedData = true;
        failAsError(m_handshake->failureReason());
        return false;
    }
    if (m_handshake->mode() != WebSocketHandshake::Connected)
        return false;

    return processFrame();
}

void MainThreadWebSocketChannel::resumeTimerFired(Timer<MainThreadWebSocketChannel>* timer)
{
    ASSERT_UNUSED(timer, timer == &m_resumeTimer);

    RefPtr<MainThreadWebSocketChannel> protect(this); // The client can close the channel, potentially removing the last reference.
    while (!m_suspended && m_client && !m_buffer.isEmpty())
        if (!processBuffer())
            break;
    if (!m_suspended && m_client && (m_state == ChannelClosed) && m_handle)
        didCloseSocketStream(m_handle.get());
}

void MainThreadWebSocketChannel::startClosingHandshake(int code, const String& reason)
{
    LOG(Network, "MainThreadWebSocketChannel %p startClosingHandshake() code=%d m_state=%d m_receivedClosingHandshake=%d", this, code, m_state, m_receivedClosingHandshake);
    if (m_state == ChannelClosing || m_state == ChannelClosed)
        return;
    ASSERT(m_handle);

    Vector<char> buf;
    if (!m_receivedClosingHandshake && code != CloseEventCodeNotSpecified) {
        unsigned char highByte = code >> 8;
        unsigned char lowByte = code;
        buf.append(static_cast<char>(highByte));
        buf.append(static_cast<char>(lowByte));
        buf.append(reason.utf8().data(), reason.utf8().length());
    }
    enqueueRawFrame(WebSocketFrame::OpCodeClose, buf.data(), buf.size());
    processOutgoingFrameQueue();

    m_state = ChannelClosing;
    if (m_client)
        m_client->didStartClosingHandshake();
}

void MainThreadWebSocketChannel::closingTimerFired(Timer<MainThreadWebSocketChannel>* timer)
{
    LOG(Network, "MainThreadWebSocketChannel %p closingTimerFired()", this);
    ASSERT_UNUSED(timer, &m_closingTimer == timer);
    if (m_handle)
        m_handle->disconnect();
}


bool MainThreadWebSocketChannel::processFrame()
{
    ASSERT(!m_buffer.isEmpty());

    WebSocketFrame frame;
    const char* frameEnd;
    String errorString;
    WebSocketFrame::ParseFrameResult result = WebSocketFrame::parseFrame(m_buffer.data(), m_buffer.size(), frame, frameEnd, errorString);
    if (result == WebSocketFrame::FrameIncomplete)
        return false;
    if (result == WebSocketFrame::FrameError) {
        failAsError(errorString);
        return false;
    }

    ASSERT(m_buffer.data() < frameEnd);
    ASSERT(frameEnd <= m_buffer.data() + m_buffer.size());

    OwnPtr<InflateResultHolder> inflateResult = m_deflateFramer.inflate(frame);
    if (!inflateResult->succeeded()) {
        failAsError(inflateResult->failureReason());
        return false;
    }
    if (!m_perMessageDeflate.inflate(frame)) {
        failAsError(m_perMessageDeflate.failureReason());
        return false;
    }

    // Validate the frame data.
    if (WebSocketFrame::isReservedOpCode(frame.opCode)) {
        failAsError("Unrecognized frame opcode: " + String::number(frame.opCode));
        return false;
    }

    if (frame.compress || frame.reserved2 || frame.reserved3) {
        failAsError("One or more reserved bits are on: reserved1 = " + String::number(frame.compress) + ", reserved2 = " + String::number(frame.reserved2) + ", reserved3 = " + String::number(frame.reserved3));
        return false;
    }

    if (frame.masked) {
        failAsError("A server must not mask any frames that it sends to the client.");
        return false;
    }

    // All control frames must not be fragmented.
    if (WebSocketFrame::isControlOpCode(frame.opCode) && !frame.final) {
        failAsError("Received fragmented control frame: opcode = " + String::number(frame.opCode));
        return false;
    }

    // All control frames must have a payload of 125 bytes or less, which means the frame must not contain
    // the "extended payload length" field.
    if (WebSocketFrame::isControlOpCode(frame.opCode) && WebSocketFrame::needsExtendedLengthField(frame.payloadLength)) {
        failAsError("Received control frame having too long payload: " + String::number(frame.payloadLength) + " bytes");
        return false;
    }

    // A new data frame is received before the previous continuous frame finishes.
    // Note that control frames are allowed to come in the middle of continuous frames.
    if (m_hasContinuousFrame && frame.opCode != WebSocketFrame::OpCodeContinuation && !WebSocketFrame::isControlOpCode(frame.opCode)) {
        failAsError("Received new data frame but previous continuous frame is unfinished.");
        return false;
    }

    InspectorInstrumentation::didReceiveWebSocketFrame(m_document, m_identifier, frame);

    switch (frame.opCode) {
    case WebSocketFrame::OpCodeContinuation:
        // An unexpected continuation frame is received without any leading frame.
        if (!m_hasContinuousFrame) {
            failAsError("Received unexpected continuation frame.");
            return false;
        }
        m_continuousFrameData.append(frame.payload, frame.payloadLength);
        skipBuffer(frameEnd - m_buffer.data());
        if (frame.final) {
            // onmessage handler may eventually call the other methods of this channel,
            // so we should pretend that we have finished to read this frame and
            // make sure that the member variables are in a consistent state before
            // the handler is invoked.
            // Vector<char>::swap() is used here to clear m_continuousFrameData.
            OwnPtr<Vector<char> > continuousFrameData = adoptPtr(new Vector<char>);
            m_continuousFrameData.swap(*continuousFrameData);
            m_hasContinuousFrame = false;
            if (m_continuousFrameOpCode == WebSocketFrame::OpCodeText) {
                String message;
                if (continuousFrameData->size())
                    message = String::fromUTF8(continuousFrameData->data(), continuousFrameData->size());
                else
                    message = "";
                if (message.isNull())
                    failAsError("Could not decode a text frame as UTF-8.");
                else
                    m_client->didReceiveMessage(message);
            } else if (m_continuousFrameOpCode == WebSocketFrame::OpCodeBinary)
                m_client->didReceiveBinaryData(continuousFrameData.release());
        }
        break;

    case WebSocketFrame::OpCodeText:
        if (frame.final) {
            String message;
            if (frame.payloadLength)
                message = String::fromUTF8(frame.payload, frame.payloadLength);
            else
                message = "";
            skipBuffer(frameEnd - m_buffer.data());
            if (message.isNull())
                failAsError("Could not decode a text frame as UTF-8.");
            else
                m_client->didReceiveMessage(message);
        } else {
            m_hasContinuousFrame = true;
            m_continuousFrameOpCode = WebSocketFrame::OpCodeText;
            ASSERT(m_continuousFrameData.isEmpty());
            m_continuousFrameData.append(frame.payload, frame.payloadLength);
            skipBuffer(frameEnd - m_buffer.data());
        }
        break;

    case WebSocketFrame::OpCodeBinary:
        if (frame.final) {
            OwnPtr<Vector<char> > binaryData = adoptPtr(new Vector<char>(frame.payloadLength));
            memcpy(binaryData->data(), frame.payload, frame.payloadLength);
            skipBuffer(frameEnd - m_buffer.data());
            m_client->didReceiveBinaryData(binaryData.release());
        } else {
            m_hasContinuousFrame = true;
            m_continuousFrameOpCode = WebSocketFrame::OpCodeBinary;
            ASSERT(m_continuousFrameData.isEmpty());
            m_continuousFrameData.append(frame.payload, frame.payloadLength);
            skipBuffer(frameEnd - m_buffer.data());
        }
        break;

    case WebSocketFrame::OpCodeClose:
        if (!frame.payloadLength)
            m_closeEventCode = CloseEventCodeNoStatusRcvd;
        else if (frame.payloadLength == 1) {
            m_closeEventCode = CloseEventCodeAbnormalClosure;
            failAsError("Received a broken close frame containing an invalid size body.");
            return false;
        } else {
            unsigned char highByte = static_cast<unsigned char>(frame.payload[0]);
            unsigned char lowByte = static_cast<unsigned char>(frame.payload[1]);
            m_closeEventCode = highByte << 8 | lowByte;
            if (m_closeEventCode == CloseEventCodeNoStatusRcvd || m_closeEventCode == CloseEventCodeAbnormalClosure || m_closeEventCode == CloseEventCodeTLSHandshake) {
                m_closeEventCode = CloseEventCodeAbnormalClosure;
                failAsError("Received a broken close frame containing a reserved status code.");
                return false;
            }
        }
        if (frame.payloadLength >= 3)
            m_closeEventReason = String::fromUTF8(&frame.payload[2], frame.payloadLength - 2);
        else
            m_closeEventReason = "";
        skipBuffer(frameEnd - m_buffer.data());
        m_receivedClosingHandshake = true;
        startClosingHandshake(m_closeEventCode, m_closeEventReason);
        m_outgoingFrameQueueStatus = OutgoingFrameQueueClosing;
        processOutgoingFrameQueue();
        break;

    case WebSocketFrame::OpCodePing:
        enqueueRawFrame(WebSocketFrame::OpCodePong, frame.payload, frame.payloadLength);
        skipBuffer(frameEnd - m_buffer.data());
        processOutgoingFrameQueue();
        break;

    case WebSocketFrame::OpCodePong:
        // A server may send a pong in response to our ping, or an unsolicited pong which is not associated with
        // any specific ping. Either way, there's nothing to do on receipt of pong.
        skipBuffer(frameEnd - m_buffer.data());
        break;

    default:
        ASSERT_NOT_REACHED();
        skipBuffer(frameEnd - m_buffer.data());
        break;
    }

    m_perMessageDeflate.resetInflateBuffer();
    return !m_buffer.isEmpty();
}

void MainThreadWebSocketChannel::enqueueTextFrame(const CString& string)
{
    ASSERT(m_outgoingFrameQueueStatus == OutgoingFrameQueueOpen);
    OwnPtr<QueuedFrame> frame = adoptPtr(new QueuedFrame);
    frame->opCode = WebSocketFrame::OpCodeText;
    frame->frameType = QueuedFrameTypeString;
    frame->stringData = string;
    m_outgoingFrameQueue.append(frame.release());
}

void MainThreadWebSocketChannel::enqueueRawFrame(WebSocketFrame::OpCode opCode, const char* data, size_t dataLength)
{
    ASSERT(m_outgoingFrameQueueStatus == OutgoingFrameQueueOpen);
    OwnPtr<QueuedFrame> frame = adoptPtr(new QueuedFrame);
    frame->opCode = opCode;
    frame->frameType = QueuedFrameTypeVector;
    frame->vectorData.resize(dataLength);
    if (dataLength)
        memcpy(frame->vectorData.data(), data, dataLength);
    m_outgoingFrameQueue.append(frame.release());
}

void MainThreadWebSocketChannel::enqueueBlobFrame(WebSocketFrame::OpCode opCode, const Blob& blob)
{
    ASSERT(m_outgoingFrameQueueStatus == OutgoingFrameQueueOpen);
    OwnPtr<QueuedFrame> frame = adoptPtr(new QueuedFrame);
    frame->opCode = opCode;
    frame->frameType = QueuedFrameTypeBlob;
    frame->blobData = Blob::create(blob.url(), blob.type(), blob.size());
    m_outgoingFrameQueue.append(frame.release());
}

void MainThreadWebSocketChannel::processOutgoingFrameQueue()
{
    if (m_outgoingFrameQueueStatus == OutgoingFrameQueueClosed)
        return;

    while (!m_outgoingFrameQueue.isEmpty()) {
        OwnPtr<QueuedFrame> frame = m_outgoingFrameQueue.takeFirst();
        switch (frame->frameType) {
        case QueuedFrameTypeString: {
            if (!sendFrame(frame->opCode, frame->stringData.data(), frame->stringData.length()))
                failAsError("Failed to send WebSocket frame.");
            break;
        }

        case QueuedFrameTypeVector:
            if (!sendFrame(frame->opCode, frame->vectorData.data(), frame->vectorData.size()))
                failAsError("Failed to send WebSocket frame.");
            break;

        case QueuedFrameTypeBlob: {
            switch (m_blobLoaderStatus) {
            case BlobLoaderNotStarted:
                ref(); // Will be derefed after didFinishLoading() or didFail().
                ASSERT(!m_blobLoader);
                m_blobLoader = adoptPtr(new FileReaderLoader(FileReaderLoader::ReadAsArrayBuffer, this));
                m_blobLoaderStatus = BlobLoaderStarted;
                m_blobLoader->start(m_document, *frame->blobData);
                m_outgoingFrameQueue.prepend(frame.release());
                return;

            case BlobLoaderStarted:
            case BlobLoaderFailed:
                m_outgoingFrameQueue.prepend(frame.release());
                return;

            case BlobLoaderFinished: {
                RefPtr<ArrayBuffer> result = m_blobLoader->arrayBufferResult();
                m_blobLoader.clear();
                m_blobLoaderStatus = BlobLoaderNotStarted;
                if (!sendFrame(frame->opCode, static_cast<const char*>(result->data()), result->byteLength()))
                    failAsError("Failed to send WebSocket frame.");
                break;
            }
            }
            break;
        }

        default:
            ASSERT_NOT_REACHED();
            break;
        }
    }

    ASSERT(m_outgoingFrameQueue.isEmpty());
    if (m_outgoingFrameQueueStatus == OutgoingFrameQueueClosing) {
        m_outgoingFrameQueueStatus = OutgoingFrameQueueClosed;
        m_handle->close();
    }
}

void MainThreadWebSocketChannel::abortOutgoingFrameQueue()
{
    m_outgoingFrameQueue.clear();
    m_outgoingFrameQueueStatus = OutgoingFrameQueueClosed;
    if (m_blobLoaderStatus == BlobLoaderStarted) {
        m_blobLoader->cancel();
        didFail(FileError::ABORT_ERR);
    }
}

bool MainThreadWebSocketChannel::sendFrame(WebSocketFrame::OpCode opCode, const char* data, size_t dataLength)
{
    ASSERT(m_handle);
    ASSERT(!m_suspended);

    WebSocketFrame frame(opCode, data, dataLength, WebSocketFrame::Final | WebSocketFrame::Masked);
    InspectorInstrumentation::didSendWebSocketFrame(m_document, m_identifier, frame);

    OwnPtr<DeflateResultHolder> deflateResult = m_deflateFramer.deflate(frame);
    if (!deflateResult->succeeded()) {
        failAsError(deflateResult->failureReason());
        return false;
    }

    if (!m_perMessageDeflate.deflate(frame)) {
        failAsError(m_perMessageDeflate.failureReason());
        return false;
    }

    Vector<char> frameData;
    frame.makeFrameData(frameData);

    m_perMessageDeflate.resetDeflateBuffer();
    return m_handle->send(frameData.data(), frameData.size());
}

} // namespace WebCore
