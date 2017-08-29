/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "core/dom/events/ScopedEventQueue.h"

#include <memory>
#include "core/dom/events/Event.h"
#include "core/dom/events/EventDispatchMediator.h"
#include "core/dom/events/EventDispatcher.h"
#include "core/dom/events/EventTarget.h"
#include "platform/wtf/PtrUtil.h"

namespace blink {

ScopedEventQueue* ScopedEventQueue::instance_ = nullptr;

ScopedEventQueue::ScopedEventQueue() : scoping_level_(0) {}

ScopedEventQueue::~ScopedEventQueue() {
  DCHECK(!scoping_level_);
  DCHECK(!queued_event_dispatch_mediators_.size());
}

void ScopedEventQueue::Initialize() {
  DCHECK(!instance_);
  std::unique_ptr<ScopedEventQueue> instance =
      WTF::WrapUnique(new ScopedEventQueue);
  instance_ = instance.release();
}

void ScopedEventQueue::EnqueueEventDispatchMediator(
    EventDispatchMediator* mediator) {
  if (ShouldQueueEvents())
    queued_event_dispatch_mediators_.push_back(mediator);
  else
    DispatchEvent(mediator);
}

void ScopedEventQueue::DispatchAllEvents() {
  HeapVector<Member<EventDispatchMediator>> queued_event_dispatch_mediators;
  queued_event_dispatch_mediators.swap(queued_event_dispatch_mediators_);

  for (auto& mediator : queued_event_dispatch_mediators)
    DispatchEvent(mediator.Release());
}

void ScopedEventQueue::DispatchEvent(EventDispatchMediator* mediator) const {
  DCHECK(mediator->GetEvent().target());
  Node* node = mediator->GetEvent().target()->ToNode();
  EventDispatcher::DispatchEvent(*node, mediator);
}

ScopedEventQueue* ScopedEventQueue::Instance() {
  if (!instance_)
    Initialize();

  return instance_;
}

void ScopedEventQueue::IncrementScopingLevel() {
  scoping_level_++;
}

void ScopedEventQueue::DecrementScopingLevel() {
  DCHECK(scoping_level_);
  scoping_level_--;
  if (!scoping_level_)
    DispatchAllEvents();
}

}  // namespace blink
