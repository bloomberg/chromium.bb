// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RTCVoidRequestPromiseImpl_h
#define RTCVoidRequestPromiseImpl_h

#include "platform/peerconnection/RTCVoidRequest.h"
#include "platform/wtf/text/WTFString.h"

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
  void RequestFailed(const String& error) override;

  virtual void Trace(blink::Visitor*);

 private:
  RTCVoidRequestPromiseImpl(RTCPeerConnection*, ScriptPromiseResolver*);

  void Clear();

  Member<RTCPeerConnection> requester_;
  Member<ScriptPromiseResolver> resolver_;
};

}  // namespace blink

#endif  // RTCVoidRequestPromiseImpl_h
