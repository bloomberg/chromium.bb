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

#include "bindings/core/v8/V8GCController.h"
#include "core/editing/spellcheck/SpellChecker.h"
#include "core/frame/WebLocalFrameBase.h"
#include "core/workers/InProcessWorkerMessagingProxy.h"
#include "core/workers/WorkerThread.h"
#include "modules/compositorworker/AbstractAnimationWorkletThread.h"
#include "platform/InstanceCounters.h"
#include "platform/Timer.h"
#include "platform/loader/fetch/MemoryCache.h"
#include "public/platform/Platform.h"
#include "public/platform/WebThread.h"
#include "public/web/WebFrame.h"

namespace blink {

namespace {

class WebLeakDetectorImpl final : public WebLeakDetector {
  WTF_MAKE_NONCOPYABLE(WebLeakDetectorImpl);

 public:
  explicit WebLeakDetectorImpl(WebLeakDetectorClient* client)
      : client_(client),
        delayed_gc_and_report_timer_(
            Platform::Current()->CurrentThread()->GetWebTaskRunner(),
            this,
            &WebLeakDetectorImpl::DelayedGCAndReport),
        delayed_report_timer_(
            Platform::Current()->CurrentThread()->GetWebTaskRunner(),
            this,
            &WebLeakDetectorImpl::DelayedReport),
        number_of_gc_needed_(0) {
    DCHECK(client_);
  }

  ~WebLeakDetectorImpl() override {}

  void PrepareForLeakDetection(WebFrame*) override;
  void CollectGarbageAndReport() override;

 private:
  void DelayedGCAndReport(TimerBase*);
  void DelayedReport(TimerBase*);

  WebLeakDetectorClient* client_;
  TaskRunnerTimer<WebLeakDetectorImpl> delayed_gc_and_report_timer_;
  TaskRunnerTimer<WebLeakDetectorImpl> delayed_report_timer_;
  int number_of_gc_needed_;
};

void WebLeakDetectorImpl::PrepareForLeakDetection(WebFrame* frame) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::HandleScope handle_scope(isolate);

  // For example, calling isValidEmailAddress in EmailInputType.cpp with a
  // non-empty string creates a static ScriptRegexp value which holds a
  // V8PerContextData indirectly. This affects the number of V8PerContextData.
  // To ensure that context data is created, call ensureScriptRegexpContext
  // here.
  V8PerIsolateData::From(isolate)->EnsureScriptRegexpContext();

  WorkerThread::TerminateAllWorkersForTesting();
  GetMemoryCache()->EvictResources();

  // If the spellchecker is allowed to continue issuing requests while the
  // leak detector runs, leaks may flakily be reported as the requests keep
  // their associated element (and document) alive.
  //
  // Stop the spellchecker to prevent this.
  if (frame->IsWebLocalFrame()) {
    WebLocalFrameBase* local_frame = ToWebLocalFrameBase(frame);
    local_frame->GetFrame()->GetSpellChecker().PrepareForLeakDetection();
  }

  // FIXME: HTML5 Notification should be closed because notification affects the
  // result of number of DOM objects.

  V8PerIsolateData::From(isolate)->ClearScriptRegexpContext();
}

void WebLeakDetectorImpl::CollectGarbageAndReport() {
  V8GCController::CollectAllGarbageForTesting(
      V8PerIsolateData::MainThreadIsolate());
  AbstractAnimationWorkletThread::CollectAllGarbage();
  // Note: Oilpan precise GC is scheduled at the end of the event loop.

  // Task queue may contain delayed object destruction tasks.
  // This method is called from navigation hook inside FrameLoader,
  // so previous document is still held by the loader until the next event loop.
  // Complete all pending tasks before proceeding to gc.
  number_of_gc_needed_ = 2;
  delayed_gc_and_report_timer_.StartOneShot(0, BLINK_FROM_HERE);
}

void WebLeakDetectorImpl::DelayedGCAndReport(TimerBase*) {
  // We do a second and third GC here to address flakiness
  // The second GC is necessary as Resource GC may have postponed clean-up tasks
  // to next event loop.  The third GC is necessary for cleaning up Document
  // after worker object died.

  V8GCController::CollectAllGarbageForTesting(
      V8PerIsolateData::MainThreadIsolate());
  AbstractAnimationWorkletThread::CollectAllGarbage();
  // Note: Oilpan precise GC is scheduled at the end of the event loop.

  // Inspect counters on the next event loop.
  if (--number_of_gc_needed_ > 0) {
    delayed_gc_and_report_timer_.StartOneShot(0, BLINK_FROM_HERE);
  } else if (number_of_gc_needed_ > -1 &&
             InProcessWorkerMessagingProxy::ProxyCount()) {
    // It is possible that all posted tasks for finalizing in-process proxy
    // objects will not have run before the final round of GCs started. If so,
    // do yet another pass, letting these tasks run and then afterwards perform
    // a GC to tidy up.
    //
    // TODO(sof): use proxyCount() to always decide if another GC needs to be
    // scheduled.  Some debug bots running browser unit tests disagree
    // (crbug.com/616714)
    delayed_gc_and_report_timer_.StartOneShot(0, BLINK_FROM_HERE);
  } else {
    delayed_report_timer_.StartOneShot(0, BLINK_FROM_HERE);
  }
}

void WebLeakDetectorImpl::DelayedReport(TimerBase*) {
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

WebLeakDetector* WebLeakDetector::Create(WebLeakDetectorClient* client) {
  return new WebLeakDetectorImpl(client);
}

}  // namespace blink
