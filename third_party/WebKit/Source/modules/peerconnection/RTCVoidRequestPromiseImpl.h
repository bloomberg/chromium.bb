// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_VOID_REQUEST_PROMISE_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_VOID_REQUEST_PROMISE_IMPL_H_

#include "third_party/blink/renderer/platform/peerconnection/rtc_void_request.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class ScriptPromiseResolver;
class RTCPeerConnection;

class RTCVoidRequestPromiseImpl final : public RTCVoidRequest {
 public:
  static RTCVoidRequestPromiseImpl* Create(RTCPeerConnection*,
                                           ScriptPromiseResolver*);
  ~RTCVoidRequestPromiseImpl() override;

  // RTCVoidRequest
  void RequestSucceeded() override;
  void RequestFailed(const WebRTCError&) override;

  virtual void Trace(blink::Visitor*);

 private:
  RTCVoidRequestPromiseImpl(RTCPeerConnection*, ScriptPromiseResolver*);

  void Clear();

  Member<RTCPeerConnection> requester_;
  Member<ScriptPromiseResolver> resolver_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_VOID_REQUEST_PROMISE_IMPL_H_
