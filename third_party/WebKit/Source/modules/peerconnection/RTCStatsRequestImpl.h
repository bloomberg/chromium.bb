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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef RTCStatsRequestImpl_h
#define RTCStatsRequestImpl_h

#include "core/dom/ContextLifecycleObserver.h"
#include "modules/peerconnection/RTCStatsResponse.h"
#include "platform/heap/Handle.h"
#include "platform/peerconnection/RTCStatsRequest.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class MediaStreamTrack;
class RTCPeerConnection;
class RTCStatsCallback;

class RTCStatsRequestImpl final : public RTCStatsRequest,
                                  public ContextLifecycleObserver {
  USING_GARBAGE_COLLECTED_MIXIN(RTCStatsRequestImpl);

 public:
  static RTCStatsRequestImpl* Create(ExecutionContext*,
                                     RTCPeerConnection*,
                                     RTCStatsCallback*,
                                     MediaStreamTrack*);
  ~RTCStatsRequestImpl() override;

  RTCStatsResponseBase* CreateResponse() override;
  bool HasSelector() override;
  MediaStreamComponent* Component() override;

  void RequestSucceeded(RTCStatsResponseBase*) override;

  // ContextLifecycleObserver
  void ContextDestroyed(ExecutionContext*) override;

  virtual void Trace(blink::Visitor*);

 private:
  RTCStatsRequestImpl(ExecutionContext*,
                      RTCPeerConnection*,
                      RTCStatsCallback*,
                      MediaStreamTrack*);

  void Clear();

  Member<RTCStatsCallback> success_callback_;
  Member<MediaStreamComponent> component_;
  Member<RTCPeerConnection> requester_;
};

}  // namespace blink

#endif  // RTCStatsRequestImpl_h
