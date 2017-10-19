/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Google Inc. nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
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

#ifndef RTCVoidRequestImpl_h
#define RTCVoidRequestImpl_h

#include "core/dom/ContextLifecycleObserver.h"
#include "core/dom/ExceptionCode.h"
#include "platform/heap/Handle.h"
#include "platform/peerconnection/RTCVoidRequest.h"

namespace blink {

class RTCPeerConnection;
class RTCPeerConnectionErrorCallback;
class VoidCallback;

class RTCVoidRequestImpl final : public RTCVoidRequest,
                                 public ContextLifecycleObserver {
  USING_GARBAGE_COLLECTED_MIXIN(RTCVoidRequestImpl);

 public:
  static RTCVoidRequestImpl* Create(ExecutionContext*,
                                    RTCPeerConnection*,
                                    VoidCallback*,
                                    RTCPeerConnectionErrorCallback*);
  ~RTCVoidRequestImpl() override;

  // RTCVoidRequest
  void RequestSucceeded() override;
  void RequestFailed(const String& error) override;

  // ContextLifecycleObserver
  void ContextDestroyed(ExecutionContext*) override;

  virtual void Trace(blink::Visitor*);

 private:
  RTCVoidRequestImpl(ExecutionContext*,
                     RTCPeerConnection*,
                     VoidCallback*,
                     RTCPeerConnectionErrorCallback*);

  void Clear();

  Member<VoidCallback> success_callback_;
  Member<RTCPeerConnectionErrorCallback> error_callback_;
  Member<RTCPeerConnection> requester_;
};

}  // namespace blink

#endif  // RTCVoidRequestImpl_h
