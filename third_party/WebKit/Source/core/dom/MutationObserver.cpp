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

#include "core/dom/MutationObserver.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/Microtask.h"
#include "core/dom/MutationCallback.h"
#include "core/dom/MutationObserverInit.h"
#include "core/dom/MutationObserverRegistration.h"
#include "core/dom/MutationRecord.h"
#include "core/dom/Node.h"
#include "core/html/HTMLSlotElement.h"
#include "core/inspector/InspectorInstrumentation.h"
#include <algorithm>

namespace blink {

static unsigned s_observerPriority = 0;

struct MutationObserver::ObserverLessThan {
  bool operator()(const Member<MutationObserver>& lhs,
                  const Member<MutationObserver>& rhs) {
    return lhs->m_priority < rhs->m_priority;
  }
};

MutationObserver* MutationObserver::create(MutationCallback* callback) {
  DCHECK(isMainThread());
  return new MutationObserver(callback);
}

MutationObserver::MutationObserver(MutationCallback* callback)
    : m_callback(callback), m_priority(s_observerPriority++) {}

MutationObserver::~MutationObserver() {
  cancelInspectorAsyncTasks();
}

void MutationObserver::observe(Node* node,
                               const MutationObserverInit& observerInit,
                               ExceptionState& exceptionState) {
  DCHECK(node);

  MutationObserverOptions options = 0;

  if (observerInit.hasAttributeOldValue() && observerInit.attributeOldValue())
    options |= AttributeOldValue;

  HashSet<AtomicString> attributeFilter;
  if (observerInit.hasAttributeFilter()) {
    for (const auto& name : observerInit.attributeFilter())
      attributeFilter.insert(AtomicString(name));
    options |= AttributeFilter;
  }

  bool attributes = observerInit.hasAttributes() && observerInit.attributes();
  if (attributes ||
      (!observerInit.hasAttributes() && (observerInit.hasAttributeOldValue() ||
                                         observerInit.hasAttributeFilter())))
    options |= Attributes;

  if (observerInit.hasCharacterDataOldValue() &&
      observerInit.characterDataOldValue())
    options |= CharacterDataOldValue;

  bool characterData =
      observerInit.hasCharacterData() && observerInit.characterData();
  if (characterData || (!observerInit.hasCharacterData() &&
                        observerInit.hasCharacterDataOldValue()))
    options |= CharacterData;

  if (observerInit.childList())
    options |= ChildList;

  if (observerInit.subtree())
    options |= Subtree;

  if (!(options & Attributes)) {
    if (options & AttributeOldValue) {
      exceptionState.throwTypeError(
          "The options object may only set 'attributeOldValue' to true when "
          "'attributes' is true or not present.");
      return;
    }
    if (options & AttributeFilter) {
      exceptionState.throwTypeError(
          "The options object may only set 'attributeFilter' when 'attributes' "
          "is true or not present.");
      return;
    }
  }
  if (!((options & CharacterData) || !(options & CharacterDataOldValue))) {
    exceptionState.throwTypeError(
        "The options object may only set 'characterDataOldValue' to true when "
        "'characterData' is true or not present.");
    return;
  }

  if (!(options & (Attributes | CharacterData | ChildList))) {
    exceptionState.throwTypeError(
        "The options object must set at least one of 'attributes', "
        "'characterData', or 'childList' to true.");
    return;
  }

  node->registerMutationObserver(*this, options, attributeFilter);
}

MutationRecordVector MutationObserver::takeRecords() {
  MutationRecordVector records;
  cancelInspectorAsyncTasks();
  records.swap(m_records);
  return records;
}

void MutationObserver::disconnect() {
  cancelInspectorAsyncTasks();
  m_records.clear();
  MutationObserverRegistrationSet registrations(m_registrations);
  for (auto& registration : registrations) {
    // The registration may be already unregistered while iteration.
    // Only call unregister if it is still in the original set.
    if (m_registrations.contains(registration))
      registration->unregister();
  }
  DCHECK(m_registrations.isEmpty());
}

void MutationObserver::observationStarted(
    MutationObserverRegistration* registration) {
  DCHECK(!m_registrations.contains(registration));
  m_registrations.insert(registration);
}

void MutationObserver::observationEnded(
    MutationObserverRegistration* registration) {
  DCHECK(m_registrations.contains(registration));
  m_registrations.erase(registration);
}

static MutationObserverSet& activeMutationObservers() {
  DEFINE_STATIC_LOCAL(MutationObserverSet, activeObservers,
                      (new MutationObserverSet));
  return activeObservers;
}

using SlotChangeList = HeapVector<Member<HTMLSlotElement>>;

// TODO(hayato): We should have a SlotChangeList for each unit of related
// similar-origin browsing context.
// https://html.spec.whatwg.org/multipage/browsers.html#unit-of-related-similar-origin-browsing-contexts
static SlotChangeList& activeSlotChangeList() {
  DEFINE_STATIC_LOCAL(SlotChangeList, slotChangeList, (new SlotChangeList));
  return slotChangeList;
}

static MutationObserverSet& suspendedMutationObservers() {
  DEFINE_STATIC_LOCAL(MutationObserverSet, suspendedObservers,
                      (new MutationObserverSet));
  return suspendedObservers;
}

static void ensureEnqueueMicrotask() {
  if (activeMutationObservers().isEmpty() && activeSlotChangeList().isEmpty())
    Microtask::enqueueMicrotask(WTF::bind(&MutationObserver::deliverMutations));
}

void MutationObserver::enqueueSlotChange(HTMLSlotElement& slot) {
  DCHECK(isMainThread());
  ensureEnqueueMicrotask();
  activeSlotChangeList().push_back(&slot);
}

void MutationObserver::cleanSlotChangeList(Document& document) {
  SlotChangeList kept;
  kept.reserveCapacity(activeSlotChangeList().size());
  for (auto& slot : activeSlotChangeList()) {
    if (slot->document() != document)
      kept.push_back(slot);
  }
  activeSlotChangeList().swap(kept);
}

static void activateObserver(MutationObserver* observer) {
  ensureEnqueueMicrotask();
  activeMutationObservers().insert(observer);
}

void MutationObserver::enqueueMutationRecord(MutationRecord* mutation) {
  DCHECK(isMainThread());
  m_records.push_back(mutation);
  activateObserver(this);
  probe::asyncTaskScheduled(m_callback->getExecutionContext(), mutation->type(),
                            mutation);
}

void MutationObserver::setHasTransientRegistration() {
  DCHECK(isMainThread());
  activateObserver(this);
}

HeapHashSet<Member<Node>> MutationObserver::getObservedNodes() const {
  HeapHashSet<Member<Node>> observedNodes;
  for (const auto& registration : m_registrations)
    registration->addRegistrationNodesToSet(observedNodes);
  return observedNodes;
}

bool MutationObserver::shouldBeSuspended() const {
  return m_callback->getExecutionContext() &&
         m_callback->getExecutionContext()->isContextSuspended();
}

void MutationObserver::cancelInspectorAsyncTasks() {
  for (auto& record : m_records)
    probe::asyncTaskCanceled(m_callback->getExecutionContext(), record);
}

void MutationObserver::deliver() {
  DCHECK(!shouldBeSuspended());

  // Calling clearTransientRegistrations() can modify m_registrations, so it's
  // necessary to make a copy of the transient registrations before operating on
  // them.
  HeapVector<Member<MutationObserverRegistration>, 1> transientRegistrations;
  for (auto& registration : m_registrations) {
    if (registration->hasTransientRegistrations())
      transientRegistrations.push_back(registration);
  }
  for (const auto& registration : transientRegistrations)
    registration->clearTransientRegistrations();

  if (m_records.isEmpty())
    return;

  MutationRecordVector records;
  records.swap(m_records);

  // Report the first (earliest) stack as the async cause.
  probe::AsyncTask asyncTask(m_callback->getExecutionContext(),
                             records.front());
  m_callback->call(records, this);
}

void MutationObserver::resumeSuspendedObservers() {
  DCHECK(isMainThread());
  if (suspendedMutationObservers().isEmpty())
    return;

  MutationObserverVector suspended;
  copyToVector(suspendedMutationObservers(), suspended);
  for (const auto& observer : suspended) {
    if (!observer->shouldBeSuspended()) {
      suspendedMutationObservers().erase(observer);
      activateObserver(observer);
    }
  }
}

void MutationObserver::deliverMutations() {
  // These steps are defined in DOM Standard's "notify mutation observers".
  // https://dom.spec.whatwg.org/#notify-mutation-observers
  DCHECK(isMainThread());

  MutationObserverVector observers;
  copyToVector(activeMutationObservers(), observers);
  activeMutationObservers().clear();

  SlotChangeList slots;
  slots.swap(activeSlotChangeList());
  for (const auto& slot : slots)
    slot->clearSlotChangeEventEnqueued();

  std::sort(observers.begin(), observers.end(), ObserverLessThan());
  for (const auto& observer : observers) {
    if (observer->shouldBeSuspended())
      suspendedMutationObservers().insert(observer);
    else
      observer->deliver();
  }
  for (const auto& slot : slots)
    slot->dispatchSlotChangeEvent();
}

DEFINE_TRACE(MutationObserver) {
  visitor->trace(m_callback);
  visitor->trace(m_records);
  visitor->trace(m_registrations);
}

}  // namespace blink
