// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ResizeObserver_h
#define ResizeObserver_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "platform/heap/Handle.h"

namespace blink {

class Document;
class Element;
class ResizeObserverCallback;
class ResizeObserverController;
class ResizeObservation;

// ResizeObserver represents ResizeObserver javascript api:
// https://github.com/WICG/ResizeObserver/
class CORE_EXPORT ResizeObserver final
    : public GarbageCollectedFinalized<ResizeObserver>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static ResizeObserver* Create(Document&, ResizeObserverCallback*);

  virtual ~ResizeObserver(){};

  // API methods
  void observe(Element*);
  void unobserve(Element*);
  void disconnect();

  // Returns depth of shallowest observed node, kDepthLimit if none.
  size_t GatherObservations(size_t deeper_than);
  bool SkippedObservations() { return skipped_observations_; }
  void DeliverObservations();
  void ClearObservations();
  void ElementSizeChanged();
  bool HasElementSizeChanged() { return element_size_changed_; }
  DECLARE_TRACE();

 private:
  ResizeObserver(ResizeObserverCallback*, Document&);

  using ObservationList = HeapLinkedHashSet<WeakMember<ResizeObservation>>;

  Member<ResizeObserverCallback> callback_;
  // List of elements we are observing
  ObservationList observations_;
  // List of elements that have changes
  HeapVector<Member<ResizeObservation>> active_observations_;
  // True if observations were skipped gatherObservations
  bool skipped_observations_;
  // True if any ResizeObservation reported size change
  bool element_size_changed_;
  WeakMember<ResizeObserverController> controller_;
};

}  // namespace blink

#endif  // ResizeObserver_h
