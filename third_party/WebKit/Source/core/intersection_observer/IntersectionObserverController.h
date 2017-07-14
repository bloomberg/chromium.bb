// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IntersectionObserverController_h
#define IntersectionObserverController_h

#include "core/dom/SuspendableObject.h"
#include "core/intersection_observer/IntersectionObserver.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/HashSet.h"

// Design doc for IntersectionObserver implementation:
//   https://docs.google.com/a/google.com/document/d/1hLK0eyT5_BzyNS4OkjsnoqqFQDYCbKfyBinj94OnLiQ

namespace blink {

class Document;

class IntersectionObserverController
    : public GarbageCollectedFinalized<IntersectionObserverController>,
      public SuspendableObject {
  USING_GARBAGE_COLLECTED_MIXIN(IntersectionObserverController);

 public:
  static IntersectionObserverController* Create(Document*);
  ~IntersectionObserverController();

  void Resume() override;

  void ScheduleIntersectionObserverForDelivery(IntersectionObserver&);
  void DeliverIntersectionObservations();
  void ComputeTrackedIntersectionObservations();
  void AddTrackedObserver(IntersectionObserver&);
  void RemoveTrackedObserversForRoot(const Node&);

  DECLARE_TRACE();

 private:
  explicit IntersectionObserverController(Document*);
  void PostTaskToDeliverObservations();

 private:
  // IntersectionObservers for which this is the tracking document.
  HeapHashSet<WeakMember<IntersectionObserver>> tracked_intersection_observers_;
  // IntersectionObservers for which this is the execution context of the
  // callback.
  HeapHashSet<Member<IntersectionObserver>> pending_intersection_observers_;

  bool callback_fired_while_suspended_;
};

}  // namespace blink

#endif  // IntersectionObserverController_h
