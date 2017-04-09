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

#ifndef WebPepperSocket_h
#define WebPepperSocket_h

#include "../platform/WebCommon.h"
#include "../platform/WebPrivatePtr.h"
#include "../platform/WebString.h"

namespace blink {

class WebArrayBuffer;
class WebDocument;
class WebPepperSocketClient;
class WebURL;

class WebPepperSocket {
 public:
  enum CloseEventCode {
    kCloseEventCodeNotSpecified = -1,
    kCloseEventCodeNormalClosure = 1000,
    kCloseEventCodeGoingAway = 1001,
    kCloseEventCodeProtocolError = 1002,
    kCloseEventCodeUnsupportedData = 1003,
    kCloseEventCodeFrameTooLarge = 1004,
    kCloseEventCodeNoStatusRcvd = 1005,
    kCloseEventCodeAbnormalClosure = 1006,
    kCloseEventCodeInvalidFramePayloadData = 1007,
    kCloseEventCodePolicyViolation = 1008,
    kCloseEventCodeMessageTooBig = 1009,
    kCloseEventCodeMandatoryExt = 1010,
    kCloseEventCodeInternalError = 1011,
    kCloseEventCodeTLSHandshake = 1015,
    kCloseEventCodeMinimumUserDefined = 3000,
    kCloseEventCodeMaximumUserDefined = 4999
  };

  enum BinaryType { kBinaryTypeBlob = 0, kBinaryTypeArrayBuffer = 1 };

  BLINK_EXPORT static WebPepperSocket* Create(const WebDocument&,
                                              WebPepperSocketClient*);
  virtual ~WebPepperSocket() {}

  // These functions come from binaryType attribute of the WebSocket API
  // specification. It specifies binary object type for receiving binary
  // frames representation. Receiving text frames are always mapped to
  // WebString type regardless of this attribute.
  // Default type is BinaryTypeBlob. But currently it is not supported.
  // Set BinaryTypeArrayBuffer here ahead of using binary communication.
  // See also, The WebSocket API - http://www.w3.org/TR/websockets/ .
  virtual BinaryType GetBinaryType() const = 0;
  virtual bool SetBinaryType(BinaryType) = 0;

  virtual void Connect(const WebURL&, const WebString& protocol) = 0;
  virtual WebString Subprotocol() { return WebString(); }
  virtual WebString Extensions() { return WebString(); }
  virtual bool SendText(const WebString&) = 0;
  virtual bool SendArrayBuffer(const WebArrayBuffer&) = 0;
  virtual unsigned long BufferedAmount() const { return 0; }
  virtual void Close(int code, const WebString& reason) = 0;
  virtual void Fail(const WebString& reason) = 0;
  virtual void Disconnect() = 0;
};

}  // namespace blink

#endif  // WebPepperSocket_h
