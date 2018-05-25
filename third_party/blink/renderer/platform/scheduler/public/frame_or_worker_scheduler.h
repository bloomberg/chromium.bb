// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_FRAME_OR_WORKER_SCHEDULER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_FRAME_OR_WORKER_SCHEDULER_H_

#include <unordered_map>

#include "base/memory/weak_ptr.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator.h"

namespace blink {
class FrameScheduler;

// This is the base class of FrameScheduler and WorkerScheduler.
class PLATFORM_EXPORT FrameOrWorkerScheduler {
  USING_FAST_MALLOC(FrameOrWorkerScheduler);

 public:
  // Observer type that regulates conditions to invoke callbacks.
  enum class ObserverType { kLoader, kWorkerScheduler };

  // Represents throttling state.
  // TODO(altimin): Move it into standalone LifecycleState.
  enum class ThrottlingState {
    // Frame is active and should not be throttled.
    kNotThrottled,
    // Frame has just been backgrounded and can be throttled non-aggressively.
    kHidden,
    // Frame spent some time in background and can be fully throttled.
    kThrottled,
    // Frame is stopped, no tasks associated with it can run.
    kStopped,
  };

  static const char* ThrottlingStateToString(ThrottlingState state);

  // Observer interface to receive scheduling policy change events.
  class Observer {
   public:
    virtual ~Observer() = default;

    // Notified when throttling state is changed. May be called consecutively
    // with the same value.
    virtual void OnThrottlingStateChanged(ThrottlingState) = 0;
  };

  class PLATFORM_EXPORT ThrottlingObserverHandle {
   public:
    ThrottlingObserverHandle(FrameOrWorkerScheduler* scheduler,
                             Observer* observer);
    ~ThrottlingObserverHandle();

   private:
    base::WeakPtr<FrameOrWorkerScheduler> scheduler_;
    Observer* observer_;

    DISALLOW_COPY_AND_ASSIGN(ThrottlingObserverHandle);
  };

  virtual ~FrameOrWorkerScheduler();

  class ActiveConnectionHandle {
   public:
    ActiveConnectionHandle() = default;
    virtual ~ActiveConnectionHandle() = default;

   private:
    DISALLOW_COPY_AND_ASSIGN(ActiveConnectionHandle);
  };

  // Notifies scheduler that this execution context has established an active
  // real time connection (websocket, webrtc, etc). When connection is closed
  // this handle must be destroyed.
  virtual std::unique_ptr<ActiveConnectionHandle>
  OnActiveConnectionCreated() = 0;

  // Adds an Observer instance to be notified on scheduling policy changed.
  // When an Observer is added, the initial state will be notified synchronously
  // through the Observer interface.
  // A RAII handle is returned and observer is unregistered when the handle is
  // destroyed.
  std::unique_ptr<ThrottlingObserverHandle> AddThrottlingObserver(ObserverType,
                                                                  Observer*);

  virtual FrameScheduler* ToFrameScheduler() { return nullptr; }

 protected:
  FrameOrWorkerScheduler();

  void NotifyThrottlingObservers();

  virtual ThrottlingState CalculateThrottlingState(ObserverType) const {
    return ThrottlingState::kNotThrottled;
  }

  base::WeakPtr<FrameOrWorkerScheduler> GetWeakPtr();

 private:
  void RemoveThrottlingObserver(Observer* observer);

  // Observers are not owned by the scheduler.
  std::unordered_map<Observer*, ObserverType> throttling_observers_;
  base::WeakPtrFactory<FrameOrWorkerScheduler> weak_factory_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_FRAME_OR_WORKER_SCHEDULER_H_
