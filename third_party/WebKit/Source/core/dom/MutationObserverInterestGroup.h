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

#ifndef MutationObserverInterestGroup_h
#define MutationObserverInterestGroup_h

#include "core/dom/Document.h"
#include "core/dom/MutationObserver.h"
#include "core/dom/Node.h"
#include "core/dom/QualifiedName.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/HashMap.h"

namespace blink {

class MutationObserverInterestGroup final
    : public GarbageCollected<MutationObserverInterestGroup> {
 public:
  static MutationObserverInterestGroup* CreateForChildListMutation(
      Node& target) {
    if (!target.GetDocument().HasMutationObserversOfType(
            MutationObserver::kChildList))
      return nullptr;

    MutationRecordDeliveryOptions old_value_flag = 0;
    return CreateIfNeeded(target, MutationObserver::kChildList, old_value_flag);
  }

  static MutationObserverInterestGroup* CreateForCharacterDataMutation(
      Node& target) {
    if (!target.GetDocument().HasMutationObserversOfType(
            MutationObserver::kCharacterData))
      return nullptr;

    return CreateIfNeeded(target, MutationObserver::kCharacterData,
                          MutationObserver::kCharacterDataOldValue);
  }

  static MutationObserverInterestGroup* CreateForAttributesMutation(
      Node& target,
      const QualifiedName& attribute_name) {
    if (!target.GetDocument().HasMutationObserversOfType(
            MutationObserver::kAttributes))
      return nullptr;

    return CreateIfNeeded(target, MutationObserver::kAttributes,
                          MutationObserver::kAttributeOldValue,
                          &attribute_name);
  }

  bool IsOldValueRequested();
  void EnqueueMutationRecord(MutationRecord*);

  void Trace(blink::Visitor*);

 private:
  static MutationObserverInterestGroup* CreateIfNeeded(
      Node& target,
      MutationObserver::MutationType,
      MutationRecordDeliveryOptions old_value_flag,
      const QualifiedName* attribute_name = nullptr);
  MutationObserverInterestGroup(
      HeapHashMap<Member<MutationObserver>, MutationRecordDeliveryOptions>&
          observers,
      MutationRecordDeliveryOptions old_value_flag);

  bool HasOldValue(MutationRecordDeliveryOptions options) {
    return options & old_value_flag_;
  }

  HeapHashMap<Member<MutationObserver>, MutationRecordDeliveryOptions>
      observers_;
  MutationRecordDeliveryOptions old_value_flag_;
};

}  // namespace blink

#endif  // MutationObserverInterestGroup_h
