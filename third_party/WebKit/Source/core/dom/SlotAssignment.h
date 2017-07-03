// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SlotAssignment_h
#define SlotAssignment_h

#include "core/dom/TreeOrderedMap.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/text/AtomicString.h"
#include "platform/wtf/text/AtomicStringHash.h"

namespace blink {

class HTMLSlotElement;
class Node;
class ShadowRoot;

class SlotAssignment final : public GarbageCollected<SlotAssignment> {
 public:
  static SlotAssignment* Create(ShadowRoot& owner) {
    return new SlotAssignment(owner);
  }

  // Relevant DOM Standard: https://dom.spec.whatwg.org/#find-a-slot
  HTMLSlotElement* FindSlot(const Node&);
  HTMLSlotElement* FindSlotByName(const AtomicString& slot_name);

  // DOM Standaard defines these two procedures:
  // 1. https://dom.spec.whatwg.org/#assign-a-slot
  //    void assignSlot(const Node& slottable);
  // 2. https://dom.spec.whatwg.org/#assign-slotables
  //    void assignSlotables(HTMLSlotElement&);
  // As an optimization, Blink does not implement these literally.
  // Instead, provide alternative, HTMLSlotElement::hasAssignedNodesSlow()
  // so that slotchange can be detected.

  void ResolveDistribution();
  const HeapVector<Member<HTMLSlotElement>>& Slots();

  void DidAddSlot(HTMLSlotElement&);
  void DidRemoveSlot(HTMLSlotElement&);
  void DidRenameSlot(const AtomicString& old_name, HTMLSlotElement&);
  void DidChangeHostChildSlotName(const AtomicString& old_value,
                                  const AtomicString& new_value);

  bool FindHostChildBySlotName(const AtomicString& slot_name) const;

  DECLARE_TRACE();

 private:
  explicit SlotAssignment(ShadowRoot& owner);

  enum class SlotMutationType {
    kRemoved,
    kRenamed,
  };

  void CollectSlots();
  HTMLSlotElement* GetCachedFirstSlotWithoutAccessingNodeTree(
      const AtomicString& slot_name);

  void ResolveAssignment();
  void DistributeTo(Node&, HTMLSlotElement&);

  void DidAddSlotInternal(HTMLSlotElement&);
  void DidRemoveSlotInternal(HTMLSlotElement&,
                             const AtomicString& slot_name,
                             SlotMutationType);

  HeapVector<Member<HTMLSlotElement>> slots_;
  Member<TreeOrderedMap> slot_map_;
  WeakMember<ShadowRoot> owner_;
  unsigned needs_collect_slots_ : 1;
  unsigned slot_count_ : 31;
};

}  // namespace blink

#endif  // HTMLSlotAssignment_h
