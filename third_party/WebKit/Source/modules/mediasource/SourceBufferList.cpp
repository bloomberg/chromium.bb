/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "modules/mediasource/SourceBufferList.h"

#include "core/dom/events/MediaElementEventQueue.h"
#include "modules/EventModules.h"
#include "modules/mediasource/SourceBuffer.h"

namespace blink {

SourceBufferList::SourceBufferList(ExecutionContext* context,
                                   MediaElementEventQueue* async_event_queue)
    : ContextClient(context), async_event_queue_(async_event_queue) {}

SourceBufferList::~SourceBufferList() {}

void SourceBufferList::Add(SourceBuffer* buffer) {
  list_.push_back(buffer);
  ScheduleEvent(EventTypeNames::addsourcebuffer);
}

void SourceBufferList::insert(size_t position, SourceBuffer* buffer) {
  list_.insert(position, buffer);
  ScheduleEvent(EventTypeNames::addsourcebuffer);
}

void SourceBufferList::Remove(SourceBuffer* buffer) {
  size_t index = list_.Find(buffer);
  if (index == kNotFound)
    return;
  list_.erase(index);
  ScheduleEvent(EventTypeNames::removesourcebuffer);
}

void SourceBufferList::Clear() {
  list_.clear();
  ScheduleEvent(EventTypeNames::removesourcebuffer);
}

void SourceBufferList::ScheduleEvent(const AtomicString& event_name) {
  DCHECK(async_event_queue_);

  Event* event = Event::Create(event_name);
  event->SetTarget(this);

  async_event_queue_->EnqueueEvent(BLINK_FROM_HERE, event);
}

const AtomicString& SourceBufferList::InterfaceName() const {
  return EventTargetNames::SourceBufferList;
}

DEFINE_TRACE(SourceBufferList) {
  visitor->Trace(async_event_queue_);
  visitor->Trace(list_);
  EventTargetWithInlineData::Trace(visitor);
  ContextClient::Trace(visitor);
}

}  // namespace blink
