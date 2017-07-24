// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IntersectionObserver_h
#define IntersectionObserver_h

#include "bindings/core/v8/ExceptionState.h"
#include "core/intersection_observer/IntersectionObservation.h"
#include "core/intersection_observer/IntersectionObserverEntry.h"
#include "platform/Length.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/bindings/TraceWrapperMember.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/HashSet.h"
#include "platform/wtf/Vector.h"

namespace blink {

class Document;
class Element;
class ExceptionState;
class IntersectionObserverDelegate;
class IntersectionObserverInit;

class CORE_EXPORT IntersectionObserver final
    : public GarbageCollectedFinalized<IntersectionObserver>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  using EventCallback =
      Function<void(const HeapVector<Member<IntersectionObserverEntry>>&),
               WTF::kSameThreadAffinity>;

  static IntersectionObserver* Create(const IntersectionObserverInit&,
                                      IntersectionObserverDelegate&,
                                      ExceptionState&);
  static IntersectionObserver* Create(const Vector<Length>& root_margin,
                                      const Vector<float>& thresholds,
                                      Document*,
                                      std::unique_ptr<EventCallback>,
                                      ExceptionState& = ASSERT_NO_EXCEPTION);
  static void ResumeSuspendedObservers();

  // API methods.
  void observe(Element*, ExceptionState& = ASSERT_NO_EXCEPTION);
  void unobserve(Element*, ExceptionState& = ASSERT_NO_EXCEPTION);
  void disconnect(ExceptionState& = ASSERT_NO_EXCEPTION);
  HeapVector<Member<IntersectionObserverEntry>> takeRecords(ExceptionState&);

  // API attributes.
  Element* root() const { return root_.Get(); }
  String rootMargin() const;
  const Vector<float>& thresholds() const { return thresholds_; }

  // An observer can either track intersections with an explicit root Element,
  // or with the the top-level frame's viewport (the "implicit root").  When
  // tracking the implicit root, m_root will be null, but because m_root is a
  // weak pointer, we cannot surmise that this observer tracks the implicit
  // root just because m_root is null.  Hence m_rootIsImplicit.
  bool RootIsImplicit() const { return root_is_implicit_; }

  // This is the document which is responsible for running
  // computeIntersectionObservations at frame generation time.
  Document& TrackingDocument() const;

  const Length& TopMargin() const { return top_margin_; }
  const Length& RightMargin() const { return right_margin_; }
  const Length& BottomMargin() const { return bottom_margin_; }
  const Length& LeftMargin() const { return left_margin_; }
  void ComputeIntersectionObservations();
  void EnqueueIntersectionObserverEntry(IntersectionObserverEntry&);
  unsigned FirstThresholdGreaterThan(float ratio) const;
  void Deliver();
  bool HasEntries() const { return entries_.size(); }
  const HeapLinkedHashSet<WeakMember<IntersectionObservation>>& Observations()
      const {
    return observations_;
  }

  DECLARE_TRACE();
  DECLARE_TRACE_WRAPPERS();

 private:
  explicit IntersectionObserver(IntersectionObserverDelegate&,
                                Element*,
                                const Vector<Length>& root_margin,
                                const Vector<float>& thresholds);
  void ClearWeakMembers(Visitor*);

  // Returns false if this observer has an explicit root element which has been
  // deleted; true otherwise.
  bool RootIsValid() const;

  const TraceWrapperMember<IntersectionObserverDelegate> delegate_;
  WeakMember<Element> root_;
  HeapLinkedHashSet<WeakMember<IntersectionObservation>> observations_;
  HeapVector<Member<IntersectionObserverEntry>> entries_;
  Vector<float> thresholds_;
  Length top_margin_;
  Length right_margin_;
  Length bottom_margin_;
  Length left_margin_;
  unsigned root_is_implicit_ : 1;
};

}  // namespace blink

#endif  // IntersectionObserver_h
