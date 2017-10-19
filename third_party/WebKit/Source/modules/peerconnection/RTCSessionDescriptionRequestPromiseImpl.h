// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RTCSessionDescriptionRequestPromiseImpl_h
#define RTCSessionDescriptionRequestPromiseImpl_h

#include "platform/peerconnection/RTCSessionDescriptionRequest.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class RTCPeerConnection;
class ScriptPromiseResolver;
class WebRTCSessionDescription;

class RTCSessionDescriptionRequestPromiseImpl final
    : public RTCSessionDescriptionRequest {
 public:
  static RTCSessionDescriptionRequestPromiseImpl* Create(
      RTCPeerConnection*,
      ScriptPromiseResolver*);
  ~RTCSessionDescriptionRequestPromiseImpl() override;

  // RTCSessionDescriptionRequest
  void RequestSucceeded(const WebRTCSessionDescription&) override;
  void RequestFailed(const String& error) override;

  virtual void Trace(blink::Visitor*);

 private:
  RTCSessionDescriptionRequestPromiseImpl(RTCPeerConnection*,
                                          ScriptPromiseResolver*);

  void Clear();

  Member<RTCPeerConnection> requester_;
  Member<ScriptPromiseResolver> resolver_;
};

}  // namespace blink

#endif  // RTCSessionDescriptionRequestPromiseImpl_h
