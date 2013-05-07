/*
 * Copyright (C) 2009 Google Inc.  All rights reserved.
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

#ifndef WebSocketChannel_h
#define WebSocketChannel_h

#include "core/inspector/ScriptCallFrame.h"
#include "core/inspector/ScriptCallStack.h"
#include "core/page/ConsoleTypes.h"
#include "wtf/Forward.h"
#include "wtf/Noncopyable.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/PassRefPtr.h"
#include "wtf/Vector.h"

namespace WebCore {

class Blob;
class KURL;
class ScriptExecutionContext;
class WebSocketChannelClient;

class WebSocketChannel {
    WTF_MAKE_NONCOPYABLE(WebSocketChannel);
public:
    WebSocketChannel() { }
    static PassRefPtr<WebSocketChannel> create(ScriptExecutionContext*, WebSocketChannelClient*);

    enum SendResult {
        SendSuccess,
        SendFail,
        InvalidMessage
    };

    enum CloseEventCode {
        CloseEventCodeNotSpecified = -1,
        CloseEventCodeNormalClosure = 1000,
        CloseEventCodeGoingAway = 1001,
        CloseEventCodeProtocolError = 1002,
        CloseEventCodeUnsupportedData = 1003,
        CloseEventCodeFrameTooLarge = 1004,
        CloseEventCodeNoStatusRcvd = 1005,
        CloseEventCodeAbnormalClosure = 1006,
        CloseEventCodeInvalidFramePayloadData = 1007,
        CloseEventCodePolicyViolation = 1008,
        CloseEventCodeMessageTooBig = 1009,
        CloseEventCodeMandatoryExt = 1010,
        CloseEventCodeInternalError = 1011,
        CloseEventCodeTLSHandshake = 1015,
        CloseEventCodeMinimumUserDefined = 3000,
        CloseEventCodeMaximumUserDefined = 4999
    };

    // A holder class for Vector<ScriptCallFrame> for readability.
    class CallStackWrapper {
    public:
        CallStackWrapper(const Vector<ScriptCallFrame>& frames): m_frames(frames) { }
        PassRefPtr<ScriptCallStack> createCallStack() { return ScriptCallStack::create(m_frames); }

    private:
        Vector<ScriptCallFrame> m_frames;
    };

    virtual void connect(const KURL&, const String& protocol) = 0;
    virtual String subprotocol() = 0; // Will be available after didConnect() callback is invoked.
    virtual String extensions() = 0; // Will be available after didConnect() callback is invoked.
    virtual SendResult send(const String& message) = 0;
    virtual SendResult send(const ArrayBuffer&, unsigned byteOffset, unsigned byteLength) = 0;
    virtual SendResult send(const Blob&) = 0;
    virtual unsigned long bufferedAmount() const = 0;
    virtual void close(int code, const String& reason) = 0;

    // Log the reason text and close the connection. Will call didClose().
    // The MessageLevel parameter will be used for the level of the message
    // shown at the devtool console. The CallStackWrapper parameter is
    // callstack information. it may be shown with the reason text at the
    // devtool console. If the parameter is null, the instance may fill
    // the callstack information and show it with the reason text if possible.
    virtual void fail(const String& reason, MessageLevel, PassOwnPtr<CallStackWrapper>) = 0;

    // Log the reason text and close the connection. Will call didClose().
    // The MessageLevel parameter will be used for the level of the message
    // shown at the devtool console. The instance may fill the callstack
    // information and show it with the reason text if possible.
    virtual void fail(const String& reason, MessageLevel) = 0;

    virtual void disconnect() = 0; // Will suppress didClose().

    virtual void suspend() = 0;
    virtual void resume() = 0;

    void ref() { refWebSocketChannel(); }
    void deref() { derefWebSocketChannel(); }

protected:
    virtual ~WebSocketChannel() { }
    virtual void refWebSocketChannel() = 0;
    virtual void derefWebSocketChannel() = 0;
};

} // namespace WebCore

#endif // WebSocketChannel_h
