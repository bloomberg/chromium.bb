// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ResizeObserver_h
#define ResizeObserver_h

#include "core/CoreExport.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/bindings/TraceWrapperMember.h"
#include "platform/heap/Handle.h"

namespace blink {

class Document;
class Element;
class ResizeObserverController;
class ResizeObserverEntry;
class ResizeObservation;
class V8ResizeObserverCallback;

// ResizeObserver represents ResizeObserver javascript api:
// https://github.com/WICG/ResizeObserver/
class CORE_EXPORT ResizeObserver final
    : public GarbageCollectedFinalized<ResizeObserver>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // This delegate is an internal (non-web-exposed) version of ResizeCallback.
  class Delegate : public GarbageCollectedFinalized<Delegate> {
   public:
    virtual ~Delegate() = default;
    virtual void OnResize(
        const HeapVector<Member<ResizeObserverEntry>>& entries) = 0;
    virtual void Trace(blink::Visitor* visitor) {}
  };

  static ResizeObserver* Create(Document&, V8ResizeObserverCallback*);
  static ResizeObserver* Create(Document&, Delegate*);

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
  void Trace(blink::Visitor*);
  void TraceWrappers(const ScriptWrappableVisitor*) const;

 private:
  ResizeObserver(V8ResizeObserverCallback*, Document&);
  ResizeObserver(Delegate*, Document&);

  using ObservationList = HeapLinkedHashSet<WeakMember<ResizeObservation>>;

  // Either of |callback_| and |delegate_| should be non-null.
  const TraceWrapperMember<V8ResizeObserverCallback> callback_;
  const Member<Delegate> delegate_;

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
