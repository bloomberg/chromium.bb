/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 *           (C) 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2011 Andreas Kling (kling@webkit.org)
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

#include "core/events/EventListenerMap.h"

#include "core/events/EventTarget.h"
#include "platform/wtf/StdLibExtras.h"
#include "platform/wtf/Vector.h"

#if DCHECK_IS_ON()
#include "platform/wtf/Threading.h"
#include "platform/wtf/ThreadingPrimitives.h"
#endif

namespace blink {

#if DCHECK_IS_ON()
static Mutex& ActiveIteratorCountMutex() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(Mutex, mutex, ());
  return mutex;
}

void EventListenerMap::CheckNoActiveIterators() {
  MutexLocker locker(ActiveIteratorCountMutex());
  DCHECK(!active_iterator_count_);
}
#endif

EventListenerMap::EventListenerMap() {}

bool EventListenerMap::Contains(const AtomicString& event_type) const {
  for (const auto& entry : entries_) {
    if (entry.first == event_type)
      return true;
  }
  return false;
}

bool EventListenerMap::ContainsCapturing(const AtomicString& event_type) const {
  for (const auto& entry : entries_) {
    if (entry.first == event_type) {
      for (const auto& event_listener : *entry.second) {
        if (event_listener.Capture())
          return true;
      }
      return false;
    }
  }
  return false;
}

void EventListenerMap::Clear() {
  CheckNoActiveIterators();

  entries_.clear();
}

Vector<AtomicString> EventListenerMap::EventTypes() const {
  Vector<AtomicString> types;
  types.ReserveInitialCapacity(entries_.size());

  for (const auto& entry : entries_)
    types.UncheckedAppend(entry.first);

  return types;
}

static bool AddListenerToVector(EventListenerVector* vector,
                                EventListener* listener,
                                const AddEventListenerOptionsResolved& options,
                                RegisteredEventListener* registered_listener) {
  *registered_listener = RegisteredEventListener(listener, options);

  if (vector->Find(*registered_listener) != kNotFound)
    return false;  // Duplicate listener.

  vector->push_back(*registered_listener);
  return true;
}

bool EventListenerMap::Add(const AtomicString& event_type,
                           EventListener* listener,
                           const AddEventListenerOptionsResolved& options,
                           RegisteredEventListener* registered_listener) {
  CheckNoActiveIterators();

  for (const auto& entry : entries_) {
    if (entry.first == event_type)
      return AddListenerToVector(entry.second.Get(), listener, options,
                                 registered_listener);
  }

  entries_.push_back(std::make_pair(event_type, new EventListenerVector));
  return AddListenerToVector(entries_.back().second.Get(), listener, options,
                             registered_listener);
}

static bool RemoveListenerFromVector(
    EventListenerVector* listener_vector,
    const EventListener* listener,
    const EventListenerOptions& options,
    size_t* index_of_removed_listener,
    RegisteredEventListener* registered_listener) {
  const auto begin = listener_vector->data();
  const auto end = begin + listener_vector->size();

  // Do a manual search for the matching RegisteredEventListener. It is not
  // possible to create a RegisteredEventListener on the stack because of the
  // const on |listener|.
  const auto it = std::find_if(
      begin, end,
      [listener, options](const RegisteredEventListener& event_listener)
          -> bool { return event_listener.Matches(listener, options); });

  if (it == end) {
    *index_of_removed_listener = kNotFound;
    return false;
  }
  *registered_listener = *it;
  *index_of_removed_listener = it - begin;
  listener_vector->erase(*index_of_removed_listener);
  return true;
}

bool EventListenerMap::Remove(const AtomicString& event_type,
                              const EventListener* listener,
                              const EventListenerOptions& options,
                              size_t* index_of_removed_listener,
                              RegisteredEventListener* registered_listener) {
  CheckNoActiveIterators();

  for (unsigned i = 0; i < entries_.size(); ++i) {
    if (entries_[i].first == event_type) {
      bool was_removed = RemoveListenerFromVector(
          entries_[i].second.Get(), listener, options,
          index_of_removed_listener, registered_listener);
      if (entries_[i].second->IsEmpty())
        entries_.erase(i);
      return was_removed;
    }
  }

  return false;
}

EventListenerVector* EventListenerMap::Find(const AtomicString& event_type) {
  CheckNoActiveIterators();

  for (const auto& entry : entries_) {
    if (entry.first == event_type)
      return entry.second.Get();
  }

  return nullptr;
}

static void CopyListenersNotCreatedFromMarkupToTarget(
    const AtomicString& event_type,
    EventListenerVector* listener_vector,
    EventTarget* target) {
  for (auto& event_listener : *listener_vector) {
    if (event_listener.Listener()->WasCreatedFromMarkup())
      continue;
    AddEventListenerOptionsResolved options = event_listener.Options();
    target->addEventListener(event_type, event_listener.Listener(), options);
  }
}

void EventListenerMap::CopyEventListenersNotCreatedFromMarkupToTarget(
    EventTarget* target) {
  CheckNoActiveIterators();

  for (const auto& event_listener : entries_) {
    CopyListenersNotCreatedFromMarkupToTarget(
        event_listener.first, event_listener.second.Get(), target);
  }
}

DEFINE_TRACE(EventListenerMap) {
  visitor->Trace(entries_);
}

EventListenerIterator::EventListenerIterator(EventTarget* target)
    : map_(nullptr), entry_index_(0), index_(0) {
  DCHECK(target);
  EventTargetData* data = target->GetEventTargetData();

  if (!data)
    return;

  map_ = &data->event_listener_map;

#if DCHECK_IS_ON()
  {
    MutexLocker locker(ActiveIteratorCountMutex());
    map_->active_iterator_count_++;
  }
#endif
}

#if DCHECK_IS_ON()
EventListenerIterator::~EventListenerIterator() {
  if (map_) {
    MutexLocker locker(ActiveIteratorCountMutex());
    map_->active_iterator_count_--;
  }
}
#endif

EventListener* EventListenerIterator::NextListener() {
  if (!map_)
    return nullptr;

  for (; entry_index_ < map_->entries_.size(); ++entry_index_) {
    EventListenerVector& listeners = *map_->entries_[entry_index_].second;
    if (index_ < listeners.size())
      return listeners[index_++].Listener();
    index_ = 0;
  }

  return nullptr;
}

}  // namespace blink
