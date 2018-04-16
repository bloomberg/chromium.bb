// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/leak_detector/blink_leak_detector.h"

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_gc_controller.h"
#include "third_party/blink/renderer/core/core_initializer.h"
#include "third_party/blink/renderer/core/css/css_default_style_sheets.h"
#include "third_party/blink/renderer/core/editing/spellcheck/spell_checker.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/leak_detector/blink_leak_detector_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/workers/dedicated_worker_messaging_proxy.h"
#include "third_party/blink/renderer/core/workers/worker_thread.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/loader/fetch/memory_cache.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/timer.h"

namespace blink {

BlinkLeakDetector& BlinkLeakDetector::Instance() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(BlinkLeakDetector, blink_leak_detector, ());
  return blink_leak_detector;
}

BlinkLeakDetector::BlinkLeakDetector()
    : delayed_gc_timer_(Platform::Current()->CurrentThread()->GetTaskRunner(),
                        this,
                        &BlinkLeakDetector::TimerFiredGC),
      number_of_gc_needed_(0),
      client_(nullptr) {}

BlinkLeakDetector::~BlinkLeakDetector() = default;

void BlinkLeakDetector::PrepareForLeakDetection() {
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
  // Currently PrepareForLeakDetection takes frame to get the spellchecker,
  // but in the future when leak detection runs with multiple frames,
  // this code must be refactored so that it iterates thru all the frames.
  for (Page* page : Page::OrdinaryPages()) {
    for (Frame* frame = page->MainFrame(); frame;
         frame = frame->Tree().TraverseNext()) {
      if (!frame->IsLocalFrame())
        continue;
      ToLocalFrame(frame)->GetSpellChecker().PrepareForLeakDetection();
    }
  }

  // FIXME: HTML5 Notification should be closed because notification affects
  // the result of number of DOM objects.
  V8PerIsolateData::From(isolate)->ClearScriptRegexpContext();

  // Clear lazily loaded style sheets.
  CSSDefaultStyleSheets::Instance().PrepareForLeakDetection();

  // Stop keepalive loaders that may persist after page navigation.
  for (auto resource_fetcher : resource_fetchers_)
    resource_fetcher->PrepareForLeakDetection();
}

void BlinkLeakDetector::CollectGarbage() {
  V8GCController::CollectAllGarbageForTesting(
      V8PerIsolateData::MainThreadIsolate());
  CoreInitializer::GetInstance().CollectAllGarbageForAnimationWorklet();
  // Note: Oilpan precise GC is scheduled at the end of the event loop.

  // Task queue may contain delayed object destruction tasks.
  // This method is called from navigation hook inside FrameLoader,
  // so previous document is still held by the loader until the next event loop.
  // Complete all pending tasks before proceeding to gc.
  number_of_gc_needed_ = 2;
  delayed_gc_timer_.StartOneShot(TimeDelta(), FROM_HERE);
}

void BlinkLeakDetector::TimerFiredGC(TimerBase*) {
  // We do a second and third GC here to address flakiness
  // The second GC is necessary as Resource GC may have postponed clean-up tasks
  // to next event loop.  The third GC is necessary for cleaning up Document
  // after worker object died.

  // Inspect counters on the next event loop.
  if (--number_of_gc_needed_ > 0) {
    delayed_gc_timer_.StartOneShot(TimeDelta(), FROM_HERE);
  } else if (number_of_gc_needed_ > -1 &&
             DedicatedWorkerMessagingProxy::ProxyCount()) {
    // It is possible that all posted tasks for finalizing in-process proxy
    // objects will not have run before the final round of GCs started. If so,
    // do yet another pass, letting these tasks run and then afterwards perform
    // a GC to tidy up.
    //
    // TODO(sof): use proxyCount() to always decide if another GC needs to be
    // scheduled.  Some debug bots running browser unit tests disagree
    // (crbug.com/616714)
    delayed_gc_timer_.StartOneShot(TimeDelta(), FROM_HERE);
  } else {
    DCHECK(client_);
    client_->OnLeakDetectionComplete();
  }

  V8GCController::CollectAllGarbageForTesting(
      V8PerIsolateData::MainThreadIsolate());
  CoreInitializer::GetInstance().CollectAllGarbageForAnimationWorklet();
  // Note: Oilpan precise GC is scheduled at the end of the event loop.
}

void BlinkLeakDetector::SetClient(BlinkLeakDetectorClient* client) {
  client_ = client;
}

void BlinkLeakDetector::RegisterResourceFetcher(ResourceFetcher* fetcher) {
  DCHECK(IsMainThread());
  resource_fetchers_.insert(fetcher);
}

}  // namespace blink
