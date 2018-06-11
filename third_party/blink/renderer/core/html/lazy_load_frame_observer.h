// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_LAZY_LOAD_FRAME_OBSERVER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_LAZY_LOAD_FRAME_OBSERVER_H_

#include "third_party/blink/renderer/core/loader/frame_loader_types.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/time.h"

namespace blink {

class IntersectionObserver;
class IntersectionObserverEntry;
class HTMLFrameOwnerElement;
class ResourceRequest;
class Visitor;

class LazyLoadFrameObserver : public GarbageCollected<LazyLoadFrameObserver> {
 public:
  explicit LazyLoadFrameObserver(HTMLFrameOwnerElement&);

  void DeferLoadUntilNearViewport(const ResourceRequest&, FrameLoadType);
  bool IsLazyLoadPending() const { return lazy_load_intersection_observer_; }
  void CancelPendingLazyLoad();

  void StartTrackingVisibilityMetrics();
  void RecordMetricsOnLoadFinished();

  void Trace(blink::Visitor*);

 private:
  void LoadIfHiddenOrNearViewport(
      const ResourceRequest&,
      FrameLoadType,
      const HeapVector<Member<IntersectionObserverEntry>>&);

  void RecordMetricsOnVisibilityChanged(
      const HeapVector<Member<IntersectionObserverEntry>>&);

  void RecordVisibilityMetricsIfLoadedAndVisible();

  const Member<HTMLFrameOwnerElement> element_;

  // The intersection observer responsible for loading the frame once it's near
  // the viewport.
  Member<IntersectionObserver> lazy_load_intersection_observer_;

  // Used to record visibility-related metrics related to lazy load. This is an
  // IntersectionObserver instead of just an ElementVisibilityObserver so that
  // hidden frames can be detected in order to avoid recording metrics for them.
  Member<IntersectionObserver> visibility_metrics_observer_;

  // Keeps track of whether this frame was initially visible on the page.
  bool is_initially_above_the_fold_ = false;
  bool has_above_the_fold_been_set_ = false;

  // Set when the frame first becomes visible (i.e. appears in the viewport).
  TimeTicks time_when_first_visible_;
  // Set when the first load event is dispatched for the frame.
  TimeTicks time_when_first_load_finished_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_LAZY_LOAD_FRAME_OBSERVER_H_
