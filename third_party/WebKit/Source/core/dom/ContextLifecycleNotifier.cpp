/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 * Copyright (C) 2013 Google Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "core/dom/ContextLifecycleNotifier.h"

#include "core/dom/SuspendableObject.h"
#include "platform/wtf/AutoReset.h"

namespace blink {

void ContextLifecycleNotifier::NotifyResumingSuspendableObjects() {
  AutoReset<IterationState> scope(&iteration_state_, kAllowingNone);
  for (ContextLifecycleObserver* observer : observers_) {
    if (observer->ObserverType() !=
        ContextLifecycleObserver::kSuspendableObjectType)
      continue;
    SuspendableObject* suspendable_object =
        static_cast<SuspendableObject*>(observer);
#if DCHECK_IS_ON()
    DCHECK_EQ(suspendable_object->GetExecutionContext(), Context());
    DCHECK(suspendable_object->SuspendIfNeededCalled());
#endif
    suspendable_object->Resume();
  }
}

void ContextLifecycleNotifier::NotifySuspendingSuspendableObjects() {
  AutoReset<IterationState> scope(&iteration_state_, kAllowingNone);
  for (ContextLifecycleObserver* observer : observers_) {
    if (observer->ObserverType() !=
        ContextLifecycleObserver::kSuspendableObjectType)
      continue;
    SuspendableObject* suspendable_object =
        static_cast<SuspendableObject*>(observer);
#if DCHECK_IS_ON()
    DCHECK_EQ(suspendable_object->GetExecutionContext(), Context());
    DCHECK(suspendable_object->SuspendIfNeededCalled());
#endif
    suspendable_object->Suspend();
  }
}

unsigned ContextLifecycleNotifier::SuspendableObjectCount() const {
  DCHECK(!IsIteratingOverObservers());
  unsigned suspendable_objects = 0;
  for (ContextLifecycleObserver* observer : observers_) {
    if (observer->ObserverType() !=
        ContextLifecycleObserver::kSuspendableObjectType)
      continue;
    suspendable_objects++;
  }
  return suspendable_objects;
}

#if DCHECK_IS_ON()
bool ContextLifecycleNotifier::Contains(SuspendableObject* object) const {
  DCHECK(!IsIteratingOverObservers());
  for (ContextLifecycleObserver* observer : observers_) {
    if (observer->ObserverType() !=
        ContextLifecycleObserver::kSuspendableObjectType)
      continue;
    SuspendableObject* suspendable_object =
        static_cast<SuspendableObject*>(observer);
    if (suspendable_object == object)
      return true;
  }
  return false;
}
#endif

}  // namespace blink
