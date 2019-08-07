// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_EVENT_LOOP_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_EVENT_LOOP_H_

#include <memory>

#include "base/callback.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/deque.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"

namespace v8 {
class Isolate;
class MicrotaskQueue;
}  // namespace v8

namespace blink {
namespace scheduler {

// TODO(tzik): Implement EventLoopGroup that represents a group of reachable
// browsing contexts.

// Represents an event loop. The instance is held by ExecutionContexts.
// https://html.spec.whatwg.org/multipage/webappapis.html#event-loop
//
// Browsing contexts must share the same EventLoop if they have a chance to
// access each other synchronously.
// That is:
//  - Two Documents must share the same EventLoop if they are scriptable with
//    each other.
//  - Workers and Worklets can have its own EventLoop, as no other browsing
//    context can access it synchronously.
class PLATFORM_EXPORT EventLoop final : public WTF::RefCounted<EventLoop> {
  USING_FAST_MALLOC(EventLoop);

 public:
  // An static constructor for Workers and Worklets.
  // For Document, use EventLoopGroup to get or create the instance.
  static scoped_refptr<EventLoop> CreateForWorkerOrWorklet(
      v8::Isolate* isolate);

  // Queues |cb| to the backing v8::MicrotaskQueue.
  void EnqueueMicrotask(base::OnceClosure cb);

  // Runs pending microtasks until the queue is empty.
  void PerformMicrotaskCheckpoint();

  // Runs pending microtasks on the isolate's default MicrotaskQueue until it's
  // empty.
  static void PerformIsolateGlobalMicrotasksCheckpoint(v8::Isolate* isolate);

  // Disables or enables all controlled frames.
  void Disable();
  void Enable();

  // Returns the MicrotaskQueue instance to be associated to v8::Context. Pass
  // it to v8::Context::New().
  v8::MicrotaskQueue* microtask_queue() const { return microtask_queue_.get(); }

 private:
  friend class WTF::RefCounted<EventLoop>;

  explicit EventLoop(v8::Isolate* isolate);
  ~EventLoop();

  static void RunPendingMicrotask(void* data);

  // TODO(tzik): Add a back pointer to EventLoopGroup.

  v8::Isolate* isolate_;
  bool loop_enabled_ = true;
  Deque<base::OnceClosure> pending_microtasks_;
  std::unique_ptr<v8::MicrotaskQueue> microtask_queue_;

  DISALLOW_COPY_AND_ASSIGN(EventLoop);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_EVENT_LOOP_H_
