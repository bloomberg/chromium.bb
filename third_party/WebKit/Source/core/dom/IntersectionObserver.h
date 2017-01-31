// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IntersectionObserver_h
#define IntersectionObserver_h

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "core/dom/IntersectionObservation.h"
#include "core/dom/IntersectionObserverEntry.h"
#include "platform/Length.h"
#include "platform/heap/Handle.h"
#include "wtf/HashSet.h"
#include "wtf/Vector.h"

namespace blink {

class Document;
class Element;
class ExceptionState;
class IntersectionObserverCallback;
class IntersectionObserverInit;

class CORE_EXPORT IntersectionObserver final
    : public GarbageCollectedFinalized<IntersectionObserver>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  using EventCallback =
      Function<void(const HeapVector<Member<IntersectionObserverEntry>>&),
               WTF::SameThreadAffinity>;

  static IntersectionObserver* create(const IntersectionObserverInit&,
                                      IntersectionObserverCallback&,
                                      ExceptionState&);
  static IntersectionObserver* create(const Vector<Length>& rootMargin,
                                      const Vector<float>& thresholds,
                                      Document*,
                                      std::unique_ptr<EventCallback>,
                                      ExceptionState& = ASSERT_NO_EXCEPTION);
  static void resumeSuspendedObservers();

  // API methods.
  void observe(Element*, ExceptionState& = ASSERT_NO_EXCEPTION);
  void unobserve(Element*, ExceptionState& = ASSERT_NO_EXCEPTION);
  void disconnect(ExceptionState& = ASSERT_NO_EXCEPTION);
  HeapVector<Member<IntersectionObserverEntry>> takeRecords(ExceptionState&);

  // API attributes.
  Element* root() const { return m_root.get(); }
  String rootMargin() const;
  const Vector<float>& thresholds() const { return m_thresholds; }

  // An observer can either track intersections with an explicit root Element,
  // or with the the top-level frame's viewport (the "implicit root").  When
  // tracking the implicit root, m_root will be null, but because m_root is a
  // weak pointer, we cannot surmise that this observer tracks the implicit
  // root just because m_root is null.  Hence m_rootIsImplicit.
  bool rootIsImplicit() const { return m_rootIsImplicit; }

  // This is the document which is responsible for running
  // computeIntersectionObservations at frame generation time.
  Document& trackingDocument() const;

  const Length& topMargin() const { return m_topMargin; }
  const Length& rightMargin() const { return m_rightMargin; }
  const Length& bottomMargin() const { return m_bottomMargin; }
  const Length& leftMargin() const { return m_leftMargin; }
  void computeIntersectionObservations();
  void enqueueIntersectionObserverEntry(IntersectionObserverEntry&);
  unsigned firstThresholdGreaterThan(float ratio) const;
  void deliver();
  bool hasEntries() const { return m_entries.size(); }
  const HeapLinkedHashSet<WeakMember<IntersectionObservation>>& observations()
      const {
    return m_observations;
  }

  DECLARE_TRACE();

 private:
  explicit IntersectionObserver(IntersectionObserverCallback&,
                                Element*,
                                const Vector<Length>& rootMargin,
                                const Vector<float>& thresholds);
  void clearWeakMembers(Visitor*);

  // Returns false if this observer has an explicit root element which has been
  // deleted; true otherwise.
  bool rootIsValid() const;

  Member<IntersectionObserverCallback> m_callback;
  WeakMember<Element> m_root;
  HeapLinkedHashSet<WeakMember<IntersectionObservation>> m_observations;
  HeapVector<Member<IntersectionObserverEntry>> m_entries;
  Vector<float> m_thresholds;
  Length m_topMargin;
  Length m_rightMargin;
  Length m_bottomMargin;
  Length m_leftMargin;
  unsigned m_rootIsImplicit : 1;
};

}  // namespace blink

#endif  // IntersectionObserver_h
