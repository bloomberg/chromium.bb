/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
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

#include "public/web/WebLeakDetector.h"

#include "controller/leak_detector/BlinkLeakDetector.h"
#include "controller/leak_detector/BlinkLeakDetectorClient.h"
#include "core/frame/WebLocalFrameImpl.h"
#include "platform/InstanceCounters.h"
#include "public/web/WebFrame.h"

namespace blink {

namespace {

class WebLeakDetectorImpl final : public WebLeakDetector,
                                  public BlinkLeakDetectorClient {
  WTF_MAKE_NONCOPYABLE(WebLeakDetectorImpl);

 public:
  explicit WebLeakDetectorImpl(WebLeakDetectorClient* client,
                               WebFrame* webframe)
      : client_(client), detector_(this, webframe) {
    DCHECK(client_);
  }

  ~WebLeakDetectorImpl() override {}

  void PrepareForLeakDetection() override;
  void CollectGarbageAndReport() override;

  // BlinkLeakDetectorClient:
  void OnLeakDetectionComplete() override;

 private:
  WebLeakDetectorClient* client_;
  BlinkLeakDetector detector_;
};

void WebLeakDetectorImpl::PrepareForLeakDetection() {
  detector_.PrepareForLeakDetection();
}

void WebLeakDetectorImpl::CollectGarbageAndReport() {
  detector_.CollectGarbage();
}

void WebLeakDetectorImpl::OnLeakDetectionComplete() {
  DCHECK(client_);

  WebLeakDetectorClient::Result result;
  result.number_of_live_audio_nodes =
      InstanceCounters::CounterValue(InstanceCounters::kAudioHandlerCounter);
  result.number_of_live_documents =
      InstanceCounters::CounterValue(InstanceCounters::kDocumentCounter);
  result.number_of_live_nodes =
      InstanceCounters::CounterValue(InstanceCounters::kNodeCounter);
  result.number_of_live_layout_objects =
      InstanceCounters::CounterValue(InstanceCounters::kLayoutObjectCounter);
  result.number_of_live_resources =
      InstanceCounters::CounterValue(InstanceCounters::kResourceCounter);
  result.number_of_live_suspendable_objects = InstanceCounters::CounterValue(
      InstanceCounters::kSuspendableObjectCounter);
  result.number_of_live_script_promises =
      InstanceCounters::CounterValue(InstanceCounters::kScriptPromiseCounter);
  result.number_of_live_frames =
      InstanceCounters::CounterValue(InstanceCounters::kFrameCounter);
  result.number_of_live_v8_per_context_data = InstanceCounters::CounterValue(
      InstanceCounters::kV8PerContextDataCounter);
  result.number_of_worker_global_scopes = InstanceCounters::CounterValue(
      InstanceCounters::kWorkerGlobalScopeCounter);

  client_->OnLeakDetectionComplete(result);

#ifndef NDEBUG
  showLiveDocumentInstances();
#endif
}

}  // namespace

WebLeakDetector* WebLeakDetector::Create(WebLeakDetectorClient* client,
                                         WebFrame* webframe) {
  return new WebLeakDetectorImpl(client, webframe);
}

}  // namespace blink
