/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#ifndef MutationObserverRegistration_h
#define MutationObserverRegistration_h

#include "core/CoreExport.h"
#include "core/dom/MutationObserver.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/bindings/TraceWrapperMember.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/HashSet.h"
#include "platform/wtf/text/AtomicString.h"
#include "platform/wtf/text/AtomicStringHash.h"

namespace blink {

class QualifiedName;

class CORE_EXPORT MutationObserverRegistration final
    : public GarbageCollectedFinalized<MutationObserverRegistration>,
      public TraceWrapperBase {
 public:
  static MutationObserverRegistration* Create(
      MutationObserver&,
      Node*,
      MutationObserverOptions,
      const HashSet<AtomicString>& attribute_filter);
  ~MutationObserverRegistration();

  void ResetObservation(MutationObserverOptions,
                        const HashSet<AtomicString>& attribute_filter);
  void ObservedSubtreeNodeWillDetach(Node&);
  void ClearTransientRegistrations();
  bool HasTransientRegistrations() const {
    return transient_registration_nodes_ &&
           !transient_registration_nodes_->IsEmpty();
  }
  void Unregister();

  bool ShouldReceiveMutationFrom(Node&,
                                 MutationObserver::MutationType,
                                 const QualifiedName* attribute_name) const;
  bool IsSubtree() const { return options_ & MutationObserver::kSubtree; }

  MutationObserver& Observer() const { return *observer_; }
  MutationRecordDeliveryOptions DeliveryOptions() const {
    return options_ & (MutationObserver::kAttributeOldValue |
                       MutationObserver::kCharacterDataOldValue);
  }
  MutationObserverOptions MutationTypes() const {
    return options_ & MutationObserver::kAllMutationTypes;
  }

  void AddRegistrationNodesToSet(HeapHashSet<Member<Node>>&) const;

  void Dispose();

  void Trace(blink::Visitor*);
  DECLARE_VIRTUAL_TRACE_WRAPPERS();

 private:
  MutationObserverRegistration(MutationObserver&,
                               Node*,
                               MutationObserverOptions,
                               const HashSet<AtomicString>& attribute_filter);

  TraceWrapperMember<MutationObserver> observer_;
  WeakMember<Node> registration_node_;
  Member<Node> registration_node_keep_alive_;
  typedef HeapHashSet<Member<Node>> NodeHashSet;
  Member<NodeHashSet> transient_registration_nodes_;

  MutationObserverOptions options_;
  HashSet<AtomicString> attribute_filter_;
};

}  // namespace blink

#endif  // MutationObserverRegistration_h
